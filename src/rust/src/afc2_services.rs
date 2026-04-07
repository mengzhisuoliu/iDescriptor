use cxx_qt::Threading;
use cxx_qt_lib::{QByteArray, QList, QMap, QMapPair_QString_QVariant, QString};

use crate::{APP_DEVICE_STATE, RUNTIME, afc, run_sync};
use idevice::afc::{AfcClient, opcode::AfcFopenMode};
use once_cell::sync::Lazy;
use regex::Regex;
use std::{io::SeekFrom, pin::Pin};
use tokio::io::{AsyncReadExt, AsyncSeekExt};
use tokio::net::TcpListener;
use tokio::sync::oneshot;

#[cxx_qt::bridge(namespace = "CXX")]
mod qobject {
    #[namespace = ""]
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        include!("cxx-qt-lib/qlist.h");
        include!("cxx-qt-lib/qbytearray.h");
        include!("cxx-qt-lib/qmap.h");
        include!("cxx-qt-lib/qvariant.h");

        type QString = cxx_qt_lib::QString;
        type QList_QString = cxx_qt_lib::QList<QString>;
        type QByteArray = cxx_qt_lib::QByteArray;
        type QMap_QString_QVariant = cxx_qt_lib::QMap<cxx_qt_lib::QMapPair_QString_QVariant>;
    }

    extern "RustQt" {
        #[qobject]
        type Afc2Backend = super::RAfc2Backend;

        #[qinvokable]
        fn set_udid(self: Pin<&mut Afc2Backend>, udid: &QString);

        #[qinvokable]
        fn load_album_list(self: Pin<&mut Afc2Backend>);

        #[qinvokable]
        fn list_dir(self: &Afc2Backend, path: &QString) -> QList_QString;

        #[qinvokable]
        fn file_to_buffer(self: &Afc2Backend, file_path: &QString) -> QByteArray;

        #[qinvokable]
        fn is_directory(self: &Afc2Backend, path: &QString) -> bool;

        #[qinvokable]
        fn get_file_size(self: &Afc2Backend, path: &QString) -> i64;

        #[qinvokable]
        fn read_file_range(self: &Afc2Backend, path: &QString, offset: i64, len: i64)
        -> QByteArray;

        #[qinvokable]
        fn check_is_dir_and_list(self: &Afc2Backend, path: &QString);
        #[qsignal]
        fn check_is_dir_and_list_finished(
            self: Pin<&mut Afc2Backend>,
            success: bool,
            entries: &QMap_QString_QVariant,
        );

        #[qsignal]
        fn album_list_loaded(self: Pin<&mut Afc2Backend>, udid: QString, album_list: QList_QString);

        #[qinvokable]
        fn get_dirs_item_count(self: &Afc2Backend, dir: &QList_QString) -> i64;

        #[qinvokable]
        fn list_files_flat(self: &Afc2Backend, dir: &QString) -> QList_QString;

        #[qinvokable]
        fn start_video_stream(self: &Afc2Backend, file_path: &QString) -> QString;

        #[qinvokable]
        fn is_available(self: &Afc2Backend) -> bool;

        #[qinvokable]
        fn delete_path(self: &Afc2Backend, path: &QString) -> bool;
    }

    impl cxx_qt::Threading for Afc2Backend {}
    impl cxx_qt::Constructor<(QString,), NewArguments = (QString,)> for Afc2Backend {}
}

#[derive(Default)]
pub struct RAfc2Backend {
    udid: QString,
}
impl cxx_qt::Constructor<(QString,)> for qobject::Afc2Backend {
    type BaseArguments = ();
    type InitializeArguments = ();
    type NewArguments = (QString,);

    fn route_arguments(
        args: (QString,),
    ) -> (
        Self::NewArguments,
        Self::BaseArguments,
        Self::InitializeArguments,
    ) {
        (args, (), ())
    }

    fn new(args: (QString,)) -> RAfc2Backend {
        RAfc2Backend { udid: args.0 }
    }
}

impl qobject::Afc2Backend {
    fn get_udid(&self) -> &QString {
        use cxx_qt::CxxQtType;
        &self.rust().udid
    }

    fn set_udid(mut self: Pin<&mut Self>, udid: &QString) {
        use cxx_qt::CxxQtType;
        self.as_mut().rust_mut().udid = udid.clone();
    }

    fn is_available(self: &Self) -> bool {
        let udid_string = self.get_udid().to_string();

        run_sync(async move {
            let device = APP_DEVICE_STATE
                .lock()
                .await
                .get(udid_string.as_str())
                .cloned();
            if let Some(d) = device {
                d.afc2.is_some()
            } else {
                eprintln!("Device with UDID {} not found", udid_string);
                false
            }
        })
    }

    fn is_directory(self: &Self, path: &QString) -> bool {
        let udid_string = self.get_udid().to_string();
        let path_string = path.to_string();

        run_sync(async move {
            let afc_arc = {
                let maybe_device = APP_DEVICE_STATE
                    .lock()
                    .await
                    .get(udid_string.as_str())
                    .cloned();

                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("Device with UDID {} not found", udid_string);
                        return false;
                    }
                };

                match device.afc2 {
                    Some(afc) => afc.clone(),
                    None => {
                        eprintln!("AFC2 service not available for device {}", udid_string);
                        return false;
                    }
                }
            };

            let mut afc = afc_arc.lock().await;
            match afc.get_file_info(path_string.clone()).await {
                Ok(info) => info.st_ifmt == "S_IFDIR",
                Err(e) => {
                    eprintln!("Failed to get file info for {path_string}: {e}");
                    false
                }
            }
        })
    }

    fn list_dir(self: &Self, path: &QString) -> QList<QString> {
        let udid = self.get_udid().to_string();
        let path_str = path.to_string();
        let list = run_sync(async move {
            let afc_arc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(udid.as_str()).cloned();

                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("Device with UDID {} not found", udid);
                        return Vec::new();
                    }
                };

                match device.afc2 {
                    Some(afc) => afc.clone(),
                    None => {
                        eprintln!("AFC2 service not available for device {}", udid);
                        return Vec::new();
                    }
                }
            };

            let mut afc = afc_arc.lock().await;
            match afc.list_dir(&path_str).await {
                Ok(list) => list,
                Err(e) => {
                    eprintln!("Failed to read directory {path_str}: {e}");
                    Vec::new()
                }
            }
        });

        let mut qlist: QList<QString> = QList::default();
        for name in list {
            // ui already has up/down buttons maybe unnecessary
            if name == "." || name == ".." {
                continue;
            }
            qlist.append(QString::from(name));
        }
        qlist
    }

    fn check_is_dir_and_list(self: &Self, path: &QString) {
        let udid = self.get_udid().to_string();
        let path_str = path.to_string();
        let qt_t = self.qt_thread();

        RUNTIME.spawn(async move {
            let qt_thread = qt_t.clone();
            let afc_arc = {
                let device = APP_DEVICE_STATE.lock().await.get(udid.as_str()).cloned();
                let device = match device {
                    Some(d) => d,
                    None => {
                        eprintln!("Device with UDID {} not found", udid);
                        qt_thread
                            .queue(move |q| {
                                q.check_is_dir_and_list_finished(
                                    false,
                                    &QMap::<QMapPair_QString_QVariant>::default(),
                                );
                            })
                            .ok();
                        return;
                    }
                };

                match device.afc2 {
                    Some(afc) => afc.clone(),
                    None => {
                        eprintln!("AFC2 service not available for device {}", udid);
                        qt_thread
                            .queue(move |q| {
                                q.check_is_dir_and_list_finished(
                                    false,
                                    &QMap::<QMapPair_QString_QVariant>::default(),
                                );
                            })
                            .ok();
                        return;
                    }
                }
            };

            let mut afc = afc_arc.lock().await;
            let map_result = afc::check_is_dir_and_list(&mut afc, path_str).await;

            qt_thread
                .queue(move |q| {
                    q.check_is_dir_and_list_finished(true, &map_result);
                })
                .ok();
        });
    }

    fn load_album_list(self: Pin<&mut Self>) {
        let qt_t = self.qt_thread();
        let udid_owned = self.get_udid().clone();

        RUNTIME.spawn(async move {
            let udid_str = udid_owned.to_string();
            let afc_arc = {
                let device = APP_DEVICE_STATE
                    .lock()
                    .await
                    .get(udid_str.as_str())
                    .cloned();
                let device = match device {
                    Some(d) => d,
                    None => {
                        eprintln!("Device with UDID {} not found", udid_str);
                        return;
                    }
                };
                match device.afc2 {
                    Some(afc) => afc.clone(),
                    None => {
                        eprintln!("AFC2 service not available for device {}", udid_str);
                        return;
                    }
                }
            };

            println!("Device found: {:?}", udid_str);

            // list entries in /DCIM
            let mut afc = afc_arc.lock().await;
            let album_names = match afc.list_dir("/DCIM").await {
                Ok(list) => list,
                Err(e) => {
                    eprintln!("Failed to load /DCIM directory: {e}");
                    return;
                }
            };

            // Regexes: ^\d{3}APPLE$ and ^\d{8}$
            static RE_3DIGIT_APPLE: Lazy<Regex> =
                Lazy::new(|| Regex::new(r"^\d{3}APPLE$").unwrap());
            static RE_DATE_YYYYMMDD: Lazy<Regex> = Lazy::new(|| Regex::new(r"^\d{8}$").unwrap());

            let mut qlist_album: QList<QString> = QList::default();

            for name in album_names {
                // skip . and ..
                if name == "." || name == ".." {
                    continue;
                }

                // name filter
                let matches_name = name.contains("APPLE")
                    || RE_3DIGIT_APPLE.is_match(&name)
                    || RE_DATE_YYYYMMDD.is_match(&name);

                if !matches_name {
                    continue;
                }

                // check it's a directory
                let full_path = format!("/DCIM/{name}");
                match afc.get_file_info(full_path).await {
                    Ok(info) => {
                        if info.st_ifmt != "S_IFDIR" {
                            continue;
                        }
                    }
                    Err(_) => continue,
                };

                qlist_album.append(QString::from(name));
            }
            let qt_thread = qt_t.clone();
            qt_thread
                .queue(move |backend_qobj| {
                    backend_qobj.album_list_loaded(udid_owned.clone(), qlist_album);
                })
                .unwrap();
        });
    }

    fn file_to_buffer(&self, album_path: &QString) -> QByteArray {
        let udid = self.get_udid().to_string();
        let album_path_string = album_path.to_string();

        let data: Vec<u8> = run_sync(async move {
            let afc_arc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(udid.as_str()).cloned();

                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("file_to_buffer: device {udid} not found");
                        return Vec::new();
                    }
                };
                match device.afc2 {
                    Some(afc) => afc.clone(),
                    None => {
                        eprintln!("AFC2 service not available for device {}", udid);
                        return Vec::new();
                    }
                }
            };

            let mut afc = afc_arc.lock().await;

            let mut fd = match afc
                .open(album_path_string.clone(), AfcFopenMode::RdOnly)
                .await
            {
                Ok(f) => f,
                Err(e) => {
                    eprintln!("file_to_buffer: failed to open {album_path_string}: {e}");
                    return Vec::new();
                }
            };

            let mut buf = Vec::new();
            let mut chunk = vec![0u8; 8192];

            loop {
                let n = match fd.read(&mut chunk).await {
                    Ok(n) => n,
                    Err(e) => {
                        eprintln!("file_to_buffer: failed to read {album_path_string}: {e}");
                        buf.clear();
                        break;
                    }
                };
                if n == 0 {
                    break;
                }
                buf.extend_from_slice(&chunk[..n]);
            }
            fd.close().await.ok();
            buf
        });

        if data.is_empty() {
            QByteArray::default()
        } else {
            QByteArray::from(&data[..])
        }
    }

    fn get_file_size(self: &Self, path: &QString) -> i64 {
        let udid = self.get_udid().to_string();
        let path_string = path.to_string();

        run_sync(async move {
            let afc_arc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(udid.as_str()).cloned();

                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("file_to_buffer: device {udid} not found");
                        return -1;
                    }
                };
                match device.afc2 {
                    Some(afc) => afc.clone(),
                    None => {
                        eprintln!("AFC2 service not available for device {}", udid);
                        return -1;
                    }
                }
            };

            let mut afc = afc_arc.lock().await;

            afc::get_file_size(&mut afc, path_string)
                .await
                .map(|v| v as i64)
                .unwrap_or(-1)
        })
    }

    fn read_file_range(&self, path: &QString, offset: i64, len: i64) -> QByteArray {
        if offset < 0 || len <= 0 {
            return QByteArray::default();
        }

        let udid = self.get_udid().to_string();
        let path_string = path.to_string();

        let data: Vec<u8> = run_sync(async move {
            let afc_arc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(udid.as_str()).cloned();

                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("read_file_range: device {udid} not found");
                        return Vec::new();
                    }
                };

                match device.afc2 {
                    Some(afc) => afc.clone(),
                    None => {
                        eprintln!("AFC2 service not available for device {}", udid);
                        return Vec::new();
                    }
                }
            };

            let mut afc = afc_arc.lock().await;

            let mut fd = match afc.open(path_string.clone(), AfcFopenMode::RdOnly).await {
                Ok(f) => f,
                Err(e) => {
                    eprintln!("read_file_range: open({path_string}) failed: {e}");
                    return Vec::new();
                }
            };

            if offset > 0 {
                if let Err(e) = fd.seek(SeekFrom::Start(offset as u64)).await {
                    eprintln!("read_file_range: seek({path_string}, {offset}) failed: {e}");
                    let _ = fd.close().await;
                    return Vec::new();
                }
            }

            let mut buf = Vec::new();
            let mut remaining = len as usize;
            let mut chunk = vec![0u8; 8192];

            while remaining > 0 {
                let to_read = remaining.min(chunk.len());
                let n = match fd.read(&mut chunk[..to_read]).await {
                    Ok(n) => n,
                    Err(e) => {
                        eprintln!("read_file_range: read({path_string}) failed: {e}");
                        buf.clear();
                        break;
                    }
                };
                if n == 0 {
                    break;
                }
                buf.extend_from_slice(&chunk[..n]);
                remaining -= n;
            }

            let _ = fd.close().await;
            buf
        });

        if data.is_empty() {
            QByteArray::default()
        } else {
            QByteArray::from(&data[..])
        }
    }

    fn get_dirs_item_count(self: &Self, dirs: &QList<QString>) -> i64 {
        let udid = self.get_udid().to_string();

        let mut dir_vec: Vec<String> = Vec::new();
        for i in 0..dirs.len() {
            if let Some(qdir) = dirs.get(i) {
                dir_vec.push(qdir.to_string());
            }
        }

        run_sync(async move {
            let afc_arc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(udid.as_str()).cloned();

                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("get_dirs_item_count: device {udid} not found");
                        return -1;
                    }
                };

                match device.afc2 {
                    Some(afc) => afc.clone(),
                    None => {
                        eprintln!("AFC2 service not available for device {}", udid);
                        return -1;
                    }
                }
            };

            let mut afc = afc_arc.lock().await;
            let mut total: i64 = 0;

            for dir_str in dir_vec {
                let names = match afc.list_dir(&dir_str).await {
                    Ok(list) => list,
                    Err(e) => {
                        eprintln!("get_dirs_item_count: list_dir({dir_str}) failed: {e}");
                        continue;
                    }
                };

                let count = names
                    .into_iter()
                    .filter(|name| name != "." && name != "..")
                    .count() as i64;

                total += count;
            }

            total
        })
    }

    fn list_files_flat(self: &Self, dir: &QString) -> QList<QString> {
        let udid = self.get_udid().to_string();
        let dir_str = dir.to_string();

        let entries = run_sync(async move {
            let afc_arc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(udid.as_str()).cloned();

                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("list_files_flat: device {udid} not found");
                        return Vec::new();
                    }
                };

                match device.afc2 {
                    Some(afc) => afc.clone(),
                    None => {
                        eprintln!("AFC2 service not available for device {}", udid);
                        return Vec::new();
                    }
                }
            };

            let mut afc = afc_arc.lock().await;

            let names = match afc.list_dir(&dir_str).await {
                Ok(list) => list,
                Err(e) => {
                    eprintln!("list_files_flat: list_dir({dir_str}) failed: {e}");
                    return Vec::new();
                }
            };

            let mut files = Vec::new();
            for name in names {
                if name == "." || name == ".." {
                    continue;
                }
                let full_path = format!("{}/{}", dir_str, name);

                match afc.get_file_info(full_path.clone()).await {
                    Ok(info) => {
                        if info.st_ifmt != "S_IFDIR" {
                            files.push(full_path);
                        }
                    }
                    Err(e) => {
                        eprintln!("list_files_flat: get_file_info({full_path}) failed: {e}");
                        continue;
                    }
                }
            }
            files
        });

        let mut qlist: QList<QString> = QList::default();
        for path in entries {
            qlist.append(QString::from(path));
        }
        qlist
    }

    fn start_video_stream(&self, file_path: &QString) -> QString {
        let udid_str = self.get_udid().to_string();
        let path_str = file_path.to_string();
        let cloned_path = path_str.clone();

        eprintln!(
            "start_video_stream: request udid={} path={}",
            udid_str, cloned_path
        );

        // bind ephemeral port on localhost
        let listener = match std::net::TcpListener::bind("127.0.0.1:0") {
            Ok(l) => l,
            Err(e) => {
                eprintln!("start_video_stream: bind failed: {e}");
                return QString::default();
            }
        };
        let local_addr = match listener.local_addr() {
            Ok(a) => a,
            Err(e) => {
                eprintln!("start_video_stream: local_addr failed: {e}");
                return QString::default();
            }
        };
        listener.set_nonblocking(true).ok();

        // create Tokio TcpListener inside runtime
        let std_listener = {
            let _guard = RUNTIME.handle().enter();
            match TcpListener::from_std(listener) {
                Ok(l) => l,
                Err(e) => {
                    eprintln!("start_video_stream: from_std failed: {e}");
                    return QString::default();
                }
            }
        };

        let port = local_addr.port();

        let encoded = urlencoding::encode(&cloned_path);
        let url = format!("http://127.0.0.1:{}/{}", port, encoded);
        let url_clone = url.clone();
        let url_clone_for_log = url.clone();

        let (shutdown_tx, mut shutdown_rx) = oneshot::channel::<()>();
        let udid_for_insert = udid_str.clone();
        let url_for_insert = url.clone();
        let inserted = run_sync(async move {
            let maybe_device = APP_DEVICE_STATE.lock().await.get(&udid_for_insert).cloned();
            let device = match maybe_device {
                Some(d) => d,
                None => return false,
            };

            let mut video_streams = device.video_streams.lock().await;
            video_streams.insert(url_for_insert, shutdown_tx);
            true
        });
        if !inserted {
            eprintln!(
                "start_video_stream: failed to insert video stream for udid={} path={}",
                udid_str, cloned_path
            );
            return QString::default();
        }
        eprintln!(
            "start_video_stream: serving {} for udid={} path={}",
            url_clone, udid_str, cloned_path
        );
        // accept-loop task
        RUNTIME.spawn(async move {
            loop {
                tokio::select! {
                    _ = &mut shutdown_rx => {
                        // shutdown requested
                        eprintln!("start_video_stream: shutdown requested for {}", url_clone);
                        break;
                    }
                    accept_res = std_listener.accept() => {
                        let (socket, peer) = match accept_res {
                            Ok(s) => s,
                            Err(e) => {
                                eprintln!("start_video_stream: accept error: {e} on {}", url_clone);
                                break;
                            }
                        };
                        eprintln!("start_video_stream: accepted connection from {} on {}", peer, url_clone);

                        let udid_clone = udid_str.clone();
                        let path_clone = path_str.clone();

                        let mut afc_client = {
                            let maybe_device = APP_DEVICE_STATE.lock().await.get(&udid_clone).cloned();
                            let device = match maybe_device {
                                Some(d) => d,
                                None => {
                                    // FIXME
                                    // eprintln!(
                                    //     "handle_http_connection: device {} not found for {}",
                                    //     udid, path
                                    // );
                                    // let _ = socket
                                    //     .write_all(
                                    //         b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
                                    //     )
                                    //     .await;
                                    // let _ = socket.shutdown().await;
                                    return;
                                }
                            };
                            let provider = device.provider.lock().await;
                            match AfcClient::new_afc2(provider.as_ref()).await {
                                Ok(c) => c,
                                Err(_) => {
                                    //FIXME
                                    // eprintln!(
                                    //     "handle_http_connection: AfcClient::connect failed for {}: {:?}",
                                    //     path, e
                                    // );
                                    // let _ = socket
                                    //     .write_all(
                                    //         b"HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
                                    //     )
                                    //     .await;
                                    // let _ = socket.shutdown().await;
                                    return;
                                }
                            }
                        };


                        tokio::spawn(async move {
                            afc::handle_http_connection(&mut afc_client, path_clone, socket).await;
                        });
                    }
                }
            }
            eprintln!("start_video_stream: accept-loop exiting for {}", url_clone);
        });

        QString::from(url_clone_for_log)
    }

    fn delete_path(self: &Self, path: &QString) -> bool {
        let udid = self.get_udid().to_string();
        let path_str = path.to_string();

        run_sync(async move {
            let afc_arc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(udid.as_str()).cloned();

                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("delete_path: device {udid} not found");
                        return false;
                    }
                };

                match device.afc2 {
                    Some(afc) => afc.clone(),
                    None => {
                        eprintln!("AFC2 service not available for device {}", udid);
                        return false;
                    }
                }
            };

            let mut afc = afc_arc.lock().await;

            match afc.remove(path_str).await {
                Ok(_) => true,
                Err(e) => {
                    eprintln!("delete_path: remove_path failed: {e}");
                    false
                }
            }
        })
    }
}
