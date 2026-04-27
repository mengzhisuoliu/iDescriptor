use crate::POSSIBLE_ROOT;
use cxx_qt_lib::QString;
use idevice::{
    IdeviceError, IdeviceService, afc::AfcClient, diagnostics_relay::DiagnosticsRelayClient,
    house_arrest::HouseArrestClient, installation_proxy::InstallationProxyClient,
    provider::IdeviceProvider,
};
use plist::Dictionary as PlistDictionary;
use plist_macro::plist;
use rusqlite::Connection;
use std::path::PathBuf;

pub const PUBLIC_STAGING: &str = "PublicStaging";

pub async fn get_battery_info(diag: &mut DiagnosticsRelayClient) -> Option<PlistDictionary> {
    match diag.ioregistry(None, None, Some("IOPMPowerSource")).await {
        Ok(Some(dict)) => Some(dict),
        _ => None,
    }
}

pub async fn get_cable_info(diag: &mut DiagnosticsRelayClient) -> Option<PlistDictionary> {
    match diag
        .ioregistry(None, None, Some("AppleTriStarBuiltIn"))
        .await
    {
        Ok(Some(dict)) => Some(dict),
        _ => None,
    }
}

pub async fn detect_jailbroken(afc: &mut AfcClient) -> bool {
    match afc.list_dir(format!("{}/bin", POSSIBLE_ROOT)).await {
        Ok(vec) => vec.len() > 0,
        Err(_) => false,
    }
}

pub fn qstring_to_f64(qstring: &QString) -> Result<f64, std::num::ParseFloatError> {
    let rust_string: String = qstring.into();
    rust_string.parse::<f64>()
}

pub async fn calculate_apps_usage(
    instproxy: &mut InstallationProxyClient,
) -> Result<u64, Box<dyn std::error::Error + Send + Sync>> {
    let options = plist!({
        "ApplicationType": "User",
        "ReturnAttributes": [
            "StaticDiskUsage",
            "DynamicDiskUsage"
        ]
    });
    let apps = instproxy.browse(Some(options)).await?;
    let mut total_apps_space = 0u64;

    for app_info in apps {
        if let Some(app_dict) = app_info.as_dictionary() {
            if let Some(static_usage) = app_dict
                .get("StaticDiskUsage")
                .and_then(|v| v.as_unsigned_integer())
            {
                total_apps_space += static_usage;
            }

            if let Some(dynamic_usage) = app_dict
                .get("DynamicDiskUsage")
                .and_then(|v| v.as_unsigned_integer())
            {
                total_apps_space += dynamic_usage;
            }
        }
    }

    Ok(total_apps_space)
}

pub async fn vend_app_documents(
    provider: &dyn IdeviceProvider,
    bundle_id: &str,
) -> Result<AfcClient, idevice::IdeviceError> {
    let house_arrest_client = HouseArrestClient::connect(provider).await?;
    let afc_client = house_arrest_client.vend_documents(bundle_id).await?;
    Ok(afc_client)
}

pub fn query_gallery_usage(db_bytes: &mut Vec<u8>) -> Result<u64, rusqlite::Error> {
    // HACK: WAL -> legacy mode patch
    if db_bytes.len() > 20 && db_bytes[18] == 0x02 {
        db_bytes[18] = 0x01;
        db_bytes[19] = 0x01;
    }
    println!("Querying gallery usage for disk usage calculation...");

    // Open in-memory DB
    let conn = Connection::open_in_memory()?;

    unsafe {
        let db_ptr = rusqlite::ffi::sqlite3_deserialize(
            conn.handle(),
            b"main\0".as_ptr() as *const std::os::raw::c_char,
            db_bytes.as_mut_ptr(),
            db_bytes.len() as i64,
            db_bytes.len() as i64,
            rusqlite::ffi::SQLITE_DESERIALIZE_READONLY as u32,
        );
        if db_ptr != rusqlite::ffi::SQLITE_OK {
            return Err(rusqlite::Error::SqliteFailure(
                rusqlite::ffi::Error::new(db_ptr),
                None,
            ));
        }
    }

    let size: i64 = conn.query_row(
        "SELECT COALESCE(SUM(ZORIGINALFILESIZE), 0) FROM ZADDITIONALASSETATTRIBUTES",
        [],
        |row| row.get(0),
    )?;

    println!("Gallery usage calculated: {} bytes", size);
    Ok(size as u64)
}

pub fn get_lockdown_path() -> PathBuf {
    if let Ok(val) = std::env::var("USBMUXD_PAIRING_FILES_LOCATION") {
        if !val.is_empty() {
            eprintln!("Pulling pairing files from USBMUXD_PAIRING_FILES_LOCATION: {val}");
            return PathBuf::from(val);
        }
    }

    #[cfg(target_os = "linux")]
    {
        PathBuf::from("/var/lib/lockdown")
    }

    #[cfg(target_os = "macos")]
    {
        PathBuf::from("/var/db/lockdown")
    }

    #[cfg(target_os = "windows")]
    {
        let base = std::env::var_os("PROGRAMDATA")
            .map(PathBuf::from)
            .unwrap_or_else(|| PathBuf::from(r"C:\ProgramData"));
        base.join("Apple").join("Lockdown")
    }
}

/// Ensure `PublicStaging` exists on device via AFC
pub async fn ensure_public_staging(afc: &mut AfcClient) -> Result<(), IdeviceError> {
    // Try to stat and if it fails, create directory
    match afc.get_file_info(PUBLIC_STAGING).await {
        Ok(_) => Ok(()),
        Err(_) => afc.mk_dir(PUBLIC_STAGING).await,
    }
}
