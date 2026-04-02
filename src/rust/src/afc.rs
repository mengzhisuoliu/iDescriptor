use crate::{DeviceServices, run_sync};
use cxx_qt_lib::{QMap, QMapPair_QString_QVariant, QString, QVariant};
use idevice::{
    provider::IdeviceProvider,
    services::afc::{AfcClient, opcode::AfcFopenMode},
};
use std::{io::SeekFrom, pin::Pin};
use tokio::{
    io::{AsyncReadExt, AsyncSeekExt, AsyncWriteExt, BufWriter},
    net::TcpListener,
    sync::{Semaphore, oneshot},
};

// FIXME: resolve symlinks
pub async fn check_is_dir_and_list(
    afc: &mut AfcClient,
    path_str: String,
) -> QMap<QMapPair_QString_QVariant> {
    let mut map = QMap::<QMapPair_QString_QVariant>::default();

    match afc.list_dir(&path_str).await {
        Ok(list) => {
            for name in list {
                let full_path = format!("{}/{}", path_str, name);
                let is_dir = match afc.get_file_info(&full_path).await {
                    Ok(info) => info.st_ifmt == "S_IFDIR",
                    Err(e) => {
                        eprintln!("Failed to get file info for {full_path}: {e}");
                        false
                    }
                };
                map.insert(QString::from(name), QVariant::from(&is_dir));
            }
        }
        Err(e) => {
            eprintln!("Failed to read directory {path_str}: {e}");
        }
    }

    map
}

pub async fn get_file_size(afc: &mut AfcClient, path_str: String) -> Option<usize> {
    match afc.get_file_info(&path_str).await {
        Ok(info) => Some(info.size),
        Err(e) => {
            eprintln!("Failed to get file info for {path_str}: {e}");
            None
        }
    }
}

pub async fn handle_http_connection(
    afc: &mut AfcClient,
    path: String,
    mut socket: tokio::net::TcpStream,
) {
    use std::cmp::min;

    let mut buf = vec![0u8; 4096];
    let n = match socket.read(&mut buf).await {
        Ok(n) if n > 0 => n,
        _ => {
            eprintln!(
                "handle_http_connection: failed to read initial request for {}",
                path
            );
            return;
        }
    };

    let req_str = String::from_utf8_lossy(&buf[..n]).to_string();
    eprintln!(
        "handle_http_connection: received request head for {}: {}",
        path,
        req_str.lines().take(10).collect::<Vec<_>>().join("\\n")
    );
    let lines: Vec<&str> = req_str.split("\r\n").collect();
    if lines.is_empty() {
        eprintln!("handle_http_connection: empty request lines for {}", path);
        let _ = socket.shutdown().await;
        return;
    }

    // request line: "GET /... HTTP/1.1"
    let mut method = "GET".to_string();
    if let Some(first) = lines.first() {
        let parts: Vec<&str> = first.split_whitespace().collect();
        if parts.len() >= 1 {
            method = parts[0].to_string();
        }
    }
    eprintln!("handle_http_connection: method={} for {}", method, path);

    if method != "GET" && method != "HEAD" {
        eprintln!(
            "handle_http_connection: unsupported method {} for {}",
            method, path
        );
        //FIXME:FLUSH?
        let _ = socket
            .write_all(
                b"HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
            )
            .await;
        let _ = socket.shutdown().await;
        return;
    }

    // parse Range header
    let mut has_range = false;
    let mut range_start: i64 = 0;
    let mut range_end: i64 = -1;
    for line in &lines[1..] {
        if line.is_empty() {
            break;
        }
        if let Some(rest) = line
            .strip_prefix("Range: bytes=")
            .or_else(|| line.strip_prefix("range: bytes="))
        {
            let parts: Vec<&str> = rest.trim().split('-').collect();
            if parts.len() == 2 {
                has_range = true;
                if let Ok(s) = parts[0].trim().parse::<i64>() {
                    range_start = s;
                }
                if !parts[1].trim().is_empty() {
                    if let Ok(e) = parts[1].trim().parse::<i64>() {
                        range_end = e;
                    }
                }
            }
        }
    }
    eprintln!(
        "handle_http_connection: range parsed has_range={} start={} end={} for {}",
        has_range, range_start, range_end, path
    );

    let file_size = {
        let info = match afc.get_file_info(path.clone()).await {
            Ok(i) => i,
            Err(e) => {
                eprintln!(
                    "handle_http_connection: get_file_info({}) failed: {}",
                    path, e
                );
                let _ = socket
                    .write_all(
                        b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
                    )
                    .await;
                let _ = socket.shutdown().await;
                return;
            }
        };
        info.size as i64
    };

    eprintln!(
        "handle_http_connection: file_size={} for {}",
        file_size, path
    );
    if file_size <= 0 {
        eprintln!("handle_http_connection: invalid file_size for {}", path);
        let _ = socket
            .write_all(b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
            .await;
        let _ = socket.shutdown().await;
        return;
    }

    let mut start = 0_i64;
    let mut end = file_size - 1;

    if has_range {
        if range_start >= 0 {
            start = range_start;
        }
        if range_end >= 0 && range_end < file_size {
            end = range_end;
        }
        if start < 0 || start >= file_size || start > end {
            eprintln!(
                "handle_http_connection: range not satisfiable for {} (start={}, end={}, file_size={})",
                path, start, end, file_size
            );
            let _ = socket
                .write_all(
                    b"HTTP/1.1 416 Range Not Satisfiable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
                )
                .await;
            let _ = socket.shutdown().await;
            return;
        }
    }

    let content_len = (end - start + 1).max(0) as u64;
    let path_lower = path.to_lowercase();
    let mime_type = if path_lower.ends_with(".mp4") || path_lower.ends_with(".m4v") {
        "video/mp4"
    } else if path_lower.ends_with(".mov") {
        "video/quicktime"
    } else if path_lower.ends_with(".avi") {
        "video/x-msvideo"
    } else if path_lower.ends_with(".mkv") {
        "video/x-matroska"
    } else {
        "application/octet-stream"
    };

    let mut headers = String::new();
    if has_range {
        headers.push_str("HTTP/1.1 206 Partial Content\r\n");
        headers.push_str(&format!(
            "Content-Range: bytes {}-{}/{}\r\n",
            start, end, file_size
        ));
    } else {
        headers.push_str("HTTP/1.1 200 OK\r\n");
    }

    headers.push_str(&format!("Content-Length: {}\r\n", content_len));
    headers.push_str("Accept-Ranges: bytes\r\n");
    headers.push_str(&format!("Content-Type: {}\r\n", mime_type));
    headers.push_str("Connection: close\r\n");
    headers.push_str("Cache-Control: no-cache\r\n\r\n");

    eprintln!(
        "handle_http_connection: sending headers for {}: {}",
        path,
        headers.lines().next().unwrap_or_default()
    );
    if socket.write_all(headers.as_bytes()).await.is_err() {
        eprintln!("handle_http_connection: write headers failed for {}", path);
        let _ = socket.shutdown().await;
        return;
    }
    if socket.flush().await.is_err() {
        eprintln!(
            "handle_http_connection: flush failed after headers for {}",
            path
        );
        let _ = socket.shutdown().await;
        return;
    }

    if method == "HEAD" {
        eprintln!(
            "handle_http_connection: HEAD request completed for {}",
            path
        );
        let _ = socket.shutdown().await;
        return;
    }

    let mut fd = match afc.open(path.clone(), AfcFopenMode::RdOnly).await {
        Ok(f) => f,
        Err(e) => {
            eprintln!("handle_http_connection: open({}) failed: {}", path, e);
            let _ = socket.shutdown().await;
            return;
        }
    };

    if start > 0 {
        if let Err(e) = fd.seek(SeekFrom::Start(start as u64)).await {
            eprintln!(
                "handle_http_connection: seek({}, {}) failed: {}",
                path, start, e
            );
            let _ = fd.close().await;
            let _ = socket.shutdown().await;
            return;
        }
    }

    eprintln!(
        "handle_http_connection: streaming {} bytes ({}-{}) for {}",
        content_len, start, end, path
    );

    let mut remaining = content_len;
    // let mut chunk = vec![0u8; 64 * 1024];
    let mut writer = BufWriter::with_capacity(256 * 1024, &mut socket);
    let mut chunk = vec![0u8; 256 * 1024];

    while remaining > 0 {
        let to_read = min(chunk.len() as u64, remaining) as usize;
        let n = match fd.read(&mut chunk[..to_read]).await {
            Ok(n) => n,
            Err(e) => {
                eprintln!("handle_http_connection: read({}) failed: {}", path, e);
                break;
            }
        };
        if n == 0 {
            eprintln!("handle_http_connection: EOF while streaming {}", path);
            break;
        }
        if let Err(e) = writer.write_all(&chunk[..n]).await {
            eprintln!("handle_http_connection: write({}) failed: {}", path, e);
            break;
        }
        remaining -= n as u64;
    }

    writer.flush().await.ok();
    //drop(writer) is explicit so the borrow is released before we touch socket again.
    drop(writer);

    let _ = fd.close().await;
    let _ = socket.shutdown().await;
    eprintln!(
        "handle_http_connection: finished/closed connection for {}",
        path
    );
}
