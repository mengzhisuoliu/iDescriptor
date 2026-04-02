use futures::StreamExt;
use idevice::{
    IdeviceService,
    afc::AfcClient,
    diagnostics_relay::DiagnosticsRelayClient,
    heartbeat,
    lockdown::LockdownClient,
    pairing_file::PairingFile,
    provider::TcpProvider,
    usbmuxd::{Connection, UsbmuxdAddr, UsbmuxdConnection, UsbmuxdListenEvent},
};
use std::sync::atomic::{AtomicBool, Ordering};
use std::{any::type_name, sync::Arc};
use std::{collections::HashMap, net::IpAddr};
use tokio::sync::Mutex;

use std::future::Future;
use std::sync::mpsc;
use std::thread;
use tokio::runtime::{Builder, Runtime};
use tokio::task::JoinHandle;
use tokio::sync::oneshot;

use core::pin::Pin;
use cxx_qt::Threading;
use cxx_qt_lib::{QMap, QMapPair_QString_QVariant, QString, QVariant};

use crate::qobject::Core;
use once_cell::sync::Lazy;
use plist::{Value};
mod afc_services;
mod image_loader;
mod service_manager;
mod screenshot;
mod utils;
mod hause_arrest;
mod afc;
mod io_manager;


const POSSIBLE_ROOT: &str = "../../../../";
const APP_LABEL: &str = "iDescriptor";

// #[derive(Clone)]
pub struct DeviceServices {
    pub afc: Arc<Mutex<AfcClient>>,
    pub afc2: Option<Arc<Mutex<AfcClient>>>,
    pub diag: Arc<Mutex<DiagnosticsRelayClient>>,
    pub heartbeat_task: Option<JoinHandle<()>>,
    pub video_streams: Arc<Mutex<HashMap<String, Arc<AtomicBool>>>>,
    pub provider: Arc<Mutex<Box<dyn idevice::provider::IdeviceProvider>>>,
    pub lockdown: Arc<Mutex<LockdownClient>>,
}

impl Clone for DeviceServices {
    fn clone(&self) -> Self {
        DeviceServices {
            afc: self.afc.clone(),
            afc2: self.afc2.clone(),
            diag: self.diag.clone(),
            // FIXME?
            heartbeat_task: None,
            video_streams: self.video_streams.clone(),
            provider: self.provider.clone(),
            lockdown: self.lockdown.clone(),
        }
    }
}

pub static APP_DEVICE_STATE: Lazy<Mutex<HashMap<String, DeviceServices>>> =
    Lazy::new(|| Mutex::new(HashMap::new()));

static RUNTIME: Lazy<Runtime> = Lazy::new(|| {
    tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .unwrap()
});

static VIDEO_STREAMS: Lazy<std::sync::Mutex<HashMap<String, oneshot::Sender<()>>>> =
    Lazy::new(|| std::sync::Mutex::new(HashMap::new()));


pub fn run_sync<F, R>(fut: F) -> R
where
    F: Future<Output = R> + Send + 'static,
    R: Send + 'static,
{
    let (tx, rx) = mpsc::sync_channel(1);

    RUNTIME.spawn(async move {
        let res = fut.await;
        let _ = tx.send(res);
    });

    rx.recv().expect("Tokio runtime worker panicked")
}

#[cxx_qt::bridge(namespace = "CXX")]
mod qobject {
    #[namespace = ""]
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        include!("cxx-qt-lib/qlist.h");
        include!("cxx-qt-lib/qmap.h");

        type QString = cxx_qt_lib::QString;
        type QMap_QString_QVariant = cxx_qt_lib::QMap<cxx_qt_lib::QMapPair_QString_QVariant>;
    }

    extern "RustQt" {
        #[qobject]
        type Core = super::RCore;

        #[qinvokable]
        fn init(self: Pin<&mut Core>);

        #[qinvokable]
        fn init_wireless_device(
            self: Pin<&mut Core>,
            ip: &QString,
            pairing_file: &QString,
            mac_address: &QString,
        );

        #[qinvokable]
        fn get_pairing_files(self: Pin<&mut Core>) -> QMap_QString_QVariant;

        #[qinvokable]
        fn remove_device(self: Pin<&mut Core>, udid: &QString);

        #[qsignal]
        fn device_event(self: Pin<&mut Core>, event_type: u32, udid: &QString, info: &QString);

        #[qsignal]
        fn init_failed(self: Pin<&mut Core>, mac_address: &QString);

        #[qsignal]
        fn no_pairing_file(self: Pin<&mut Core>, mac_address: &QString);
    }
    impl cxx_qt::Threading for Core {}
}

#[derive(Default)]
pub struct RCore;

impl qobject::Core {
    fn init(self: Pin<&mut Self>)  {
        self.listen();
    }

    fn listen(self: Pin<&mut Self>) {
        let qt_t = self.qt_thread();

        thread::spawn(move || {
            let rt = Builder::new_current_thread().enable_all().build().unwrap();
            rt.block_on(async move {
                let mut device_map: HashMap<u32, String> = HashMap::new();

                loop {
                    match UsbmuxdConnection::default().await {
                        Ok(mut uc) => match uc.listen().await {
                            Ok(mut stream) => {
                                while let Some(evt) = stream.next().await {
                                    match evt {
                                        Ok(UsbmuxdListenEvent::Connected(d)) => {
                                            // ignore non-USB connections
                                            if d.connection_type != Connection::Usb {
                                                continue;
                                            }

                                            let udid = d.udid.clone();
                                            let device_id = d.device_id;
                                            device_map.insert(device_id, udid.clone());

                                            let qt_thread = qt_t.clone();
                                            tokio::spawn(async move {
                                                let already_paired = {
                                                    let mut u2 =
                                                        match UsbmuxdConnection::default().await {
                                                            Ok(u) => u,
                                                            Err(_) => return,
                                                        };

                                                    match u2.get_pair_record(&udid).await {
                                                        Ok(_) => true,
                                                        Err(_) => false,
                                                    }
                                                };

                                                // Helper: emit event with optional info
                                                async fn emit_connected(
                                                    qt_thread: cxx_qt::CxxQtThread<Core>,
                                                    udid: String,
                                                ) {
                                                    let mut uc =
                                                        match UsbmuxdConnection::default().await {
                                                            Ok(u) => u,
                                                            Err(_) => return,
                                                        };

                                                    let dev = match uc.get_device(&udid).await {
                                                        Ok(d) => d,
                                                        Err(_) => return,
                                                    };
                                                    let provider = dev.to_provider(
                                                        UsbmuxdAddr::default(),
                                                        APP_LABEL,
                                                    );

                                                    // FIXME: return if fails
                                                    let info: (String, String) =
                                                        init_idescriptor_device(
                                                            provider,
                                                            qt_thread.clone(),
                                                        )
                                                        .await
                                                        .unwrap_or_default();

                                                    let udid_for_event = info.0.clone();
                                                    let info_for_event = info.1.clone();

                                                    qt_thread
                                                        .queue(move |Core_qobj| {
                                                            Core_qobj.device_event(
                                                                1,
                                                                &QString::from(udid_for_event),
                                                                &QString::from(info_for_event),
                                                            );
                                                        })
                                                        .ok();
                                                }

                                                if already_paired {
                                                    emit_connected(qt_thread.clone(), udid).await;
                                                    return;
                                                }

                                                // pairing pending
                                                let udid_for_event = udid.clone();
                                                qt_thread
                                                    .queue(move |Core_qobj| {
                                                        Core_qobj.device_event(
                                                            3,
                                                            &QString::from(udid_for_event),
                                                            &QString::from(""),
                                                        );
                                                    })
                                                    .ok();

                                                let ev_fail_kind = 4;

                                                let mut uc2 = match UsbmuxdConnection::default()
                                                    .await
                                                {
                                                    Ok(u) => u,
                                                    Err(_) => {
                                                        let udid_for_event = udid.clone();
                                                        qt_thread
                                                            .queue(move |Core_qobj| {
                                                                Core_qobj.device_event(
                                                                    ev_fail_kind,
                                                                    &QString::from(udid_for_event),
                                                                    &QString::from(""),
                                                                );
                                                            })
                                                            .unwrap();
                                                        return;
                                                    }
                                                };

                                                let dev = match uc2.get_device(&udid).await {
                                                    Ok(d) => d,
                                                    Err(_) => {
                                                        let udid_for_event = udid.clone();
                                                        qt_thread
                                                            .queue(move |Core_qobj| {
                                                                Core_qobj.device_event(
                                                                    ev_fail_kind,
                                                                    &QString::from(udid_for_event),
                                                                    &QString::from(""),
                                                                );
                                                            })
                                                            .unwrap();
                                                        return;
                                                    }
                                                };

                                                let provider = dev.to_provider(
                                                    UsbmuxdAddr::default(),
                                                    "iDescriptor",
                                                );

                                                let mut lc = match LockdownClient::connect(
                                                    &provider,
                                                )
                                                .await
                                                {
                                                    Ok(l) => l,
                                                    Err(_) => {
                                                        let udid_for_event = udid.clone();
                                                        qt_thread
                                                            .queue(move |Core_qobj| {
                                                                Core_qobj.device_event(
                                                                    ev_fail_kind,
                                                                    &QString::from(udid_for_event),
                                                                    &QString::from(""),
                                                                );
                                                            })
                                                            .unwrap();
                                                        return;
                                                    }
                                                };

                                                let mut buid = match uc2.get_buid().await {
                                                    Ok(b) => b,
                                                    Err(_) => {
                                                        let udid_for_event = udid.clone();
                                                        qt_thread
                                                            .queue(move |Core_qobj| {
                                                                Core_qobj.device_event(
                                                                    ev_fail_kind,
                                                                    &QString::from(udid_for_event),
                                                                    &QString::from(""),
                                                                );
                                                            })
                                                            .unwrap();
                                                        return;
                                                    }
                                                };

                                                if let Some(first) = buid.chars().next() {
                                                    let mut chars: Vec<char> =
                                                        buid.chars().collect();
                                                    chars[0] = if first == 'F' { 'A' } else { 'F' };
                                                    buid = chars.into_iter().collect();
                                                }

                                                let host_id =
                                                    uuid::Uuid::new_v4().to_string().to_uppercase();

                                                let mut pf = match lc
                                                    .pair(host_id, buid, None)
                                                    .await
                                                {
                                                    Ok(p) => p,
                                                    Err(_) => {
                                                        let udid_for_event = udid.clone();
                                                        qt_thread
                                                            .queue(move |Core_qobj| {
                                                                Core_qobj.device_event(
                                                                    ev_fail_kind,
                                                                    &QString::from(udid_for_event),
                                                                    &QString::from(""),
                                                                );
                                                            })
                                                            .unwrap();
                                                        return;
                                                    }
                                                };
                                                pf.udid = Some(udid.clone());

                                                let bytes = match pf.serialize() {
                                                    Ok(b) => b,
                                                    Err(_) => {
                                                        let udid_for_event = udid.clone();
                                                        qt_thread
                                                            .queue(move |Core_qobj| {
                                                                Core_qobj.device_event(
                                                                    ev_fail_kind,
                                                                    &QString::from(udid_for_event),
                                                                    &QString::from(""),
                                                                );
                                                            })
                                                            .unwrap();
                                                        return;
                                                    }
                                                };

                                                if let Err(_) =
                                                    uc2.save_pair_record(&udid, bytes).await
                                                {
                                                    let udid_for_event = udid.clone();
                                                    qt_thread
                                                        .queue(move |Core_qobj| {
                                                            Core_qobj.device_event(
                                                                ev_fail_kind,
                                                                &QString::from(udid_for_event),
                                                                &QString::from(""),
                                                            );
                                                        })
                                                        .unwrap();
                                                    return;
                                                }

                                                emit_connected(qt_thread.clone(), udid).await;
                                            });
                                        }
                                        Ok(UsbmuxdListenEvent::Disconnected(device_id)) => {
                                            if let Some(udid) = device_map.remove(&device_id) {
                                                let qt_thread = qt_t.clone();
                                                qt_thread
                                                    .queue(move |Core_qobj| {
                                                        Core_qobj.device_event(
                                                            2,
                                                            &QString::from(udid),
                                                            &QString::from(""),
                                                        );
                                                    })
                                                    .unwrap();
                                            }
                                        }
                                        Err(e) => {
                                            eprintln!("usbmuxd listen error: {e:?}");
                                            break;
                                        }
                                    }
                                }
                            }
                            Err(e) => eprintln!("Failed to start usbmuxd listen: {e:?}"),
                        },
                        Err(_) => {}
                    }

                    tokio::time::sleep(std::time::Duration::from_millis(2000)).await;
                }
            });
        });
    }

    fn init_wireless_device(
        self: Pin<&mut Self>,
        ip: &QString,
        pairing_file: &QString,
        mac_address: &QString,
    ) {
        eprintln!(
            "init_wireless_device: MAC: {} IP: {} PairingFile: {}",
            mac_address, ip, pairing_file
        );
        let qt_t = self.qt_thread();
        let ip_owned = ip.to_string();
        let pairing_path = pairing_file.to_string();
        let mac_address_owned = mac_address.to_string();
        RUNTIME.spawn(async move {
            let pairing_file = match PairingFile::read_from_file(&pairing_path) {
                Ok(pf) => pf,
                Err(e) => {
                    let qt_thread = qt_t.clone();
                    qt_thread
                        .queue(move |Core_qobj| {
                            Core_qobj.no_pairing_file(&QString::from(mac_address_owned));
                        })
                        .unwrap();
                    eprintln!("Failed to read pairing file: {e}");
                    return;
                }
            };

            let t = TcpProvider {
                addr: ip_owned.parse::<IpAddr>().unwrap(),
                pairing_file: pairing_file,
                label: APP_LABEL.to_string(),
            };


             let result = tokio::select! {
                res = init_idescriptor_device(t, qt_t.clone()) => {
                    res
                }
                // timeout
                _ = tokio::time::sleep(tokio::time::Duration::from_secs(10)) => {
                    eprintln!("Timeout collecting device info for wireless device mac address: {mac_address_owned}");
                    None 
                }
            };

            match result {
                Some((udid, info)) => {
                    // emit event with info
                    let qt_thread = qt_t.clone();

                    qt_thread
                        .queue(move |Core_qobj| {
                            Core_qobj.device_event(
                                1,
                                &QString::from(udid),
                                &QString::from(info),
                            );
                        })
                        .ok();
                }
                None => {
                    eprintln!("Failed to collect device info for wireless device mac address: {mac_address_owned}");
                    let qt_thread = qt_t.clone();

                    qt_thread
                        .queue(move |Core_qobj| {
                            Core_qobj.init_failed(&QString::from(mac_address_owned));
                        })
                        .ok();
                }
            }
        });
    }
    fn get_pairing_files(self: Pin<&mut Self>) -> QMap<QMapPair_QString_QVariant> {
        let mut map = QMap::<QMapPair_QString_QVariant>::default();

        let paths =
            match std::fs::read_dir(get_lockdown_path()) {
                Ok(iter) => iter
                    .filter_map(|entry| {
                        let entry = entry.ok()?;
                        let path = entry.path();
                        if path.is_file() { Some(path) } else { None }
                    })
                    .collect::<Vec<_>>(),
                Err(_) => Vec::new(),
            };

        paths.into_iter().for_each(|path| {
            if let Ok(pf) = PairingFile::read_from_file(&path) {
                if let Some(fname) = path.file_name().and_then(|s| s.to_str()) {
                    map.insert(
                        QString::from(pf.wifi_mac_address),
                        QVariant::from(&QString::from(fname)),
                    );
                }
            }
        });

        map
    }
    fn remove_device(self: Pin<&mut Self>, udid: &QString) {
        let udid_str = udid.to_string();
        RUNTIME.spawn(async move {
            let mut state = APP_DEVICE_STATE.lock().await;
            if let Some(svc) = state.remove(&udid_str) {
                let streams = svc.video_streams.lock().await;
                for (_path, flag) in streams.iter() {
                    flag.store(true, Ordering::Relaxed);
                }
                println!("Removed device with UDID {}", udid_str);
            } else {
                eprintln!(
                    "Attempted to remove non-existent device with UDID {}",
                    udid_str
                );
            }
        });
    }
}

fn get_lockdown_path() -> String {
    #[cfg(target_os = "linux")]
    return "/var/lib/lockdown".to_string();

    #[cfg(target_os = "macos")]
    return "/var/db/lockdown".to_string();

    #[cfg(target_os = "windows")]
    return std::env::var("PROGRAMDATA").unwrap_or_else(|_| "C:\\ProgramData".to_string())
        + "/Apple/Lockdown";
}

//FIXME: dont spawn hb if init fails
async fn init_idescriptor_device<
    T: idevice::provider::IdeviceProvider + Send + Sync + Clone + 'static,
>(
    provider: T,
    qt_thread: cxx_qt::CxxQtThread<Core>,
) -> Option<(String, String)> {
    let provider_name = type_name::<T>();
    let is_wireless = provider_name == "idevice::provider::TcpProvider";

    let pf = match idevice::provider::IdeviceProvider::get_pairing_file(&provider).await {
        Ok(pf) => pf,
        Err(e) => {
            eprintln!("get_pairing_file failed: {e:?}");
            return None;
        }
    };
    eprintln!("init_idescriptor_device: Pairing file obtained.");

    let mut hb_task = None;
    let mut hb = None;

    let provider_for_hb = provider.clone();
    let qt_thread_for_hb = qt_thread.clone();

    let mut lc = match LockdownClient::connect(&provider).await {
        Ok(lc) => lc,
        Err(e) => {
            eprintln!("wireless: LockdownClient::connect failed : {e:?}");
            return None;
        }
    };

    eprintln!("init_idescriptor_device: Attempting to start Lockdown session.");
    if let Err(e) = lc.start_session(&pf).await {
        eprintln!("wireless: start_session failed: {e:?}");
        return None;
    }
    eprintln!("init_idescriptor_device: Lockdown session started.");

    eprintln!(
        "init_idescriptor_device: Attempting to get default values from Lockdown."
    );
    let mut def_vals = match lc.get_value(None, None).await {
        Ok(v) => v,
        Err(e) => {
            eprintln!("wireless: get_value(None, None) failed : {e:?}");
            return None;
        }
    };
    eprintln!("init_idescriptor_device: Default values obtained.");

    let udid = def_vals
        .as_dictionary()?
        .get("UniqueDeviceID")?
        .as_string()?
        .to_string();

    if udid.is_empty() {
        eprintln!("init_idescriptor_device: UDID is empty.");
        return None;
    }

    if is_wireless {
        eprintln!(
            "init_idescriptor_device: Attempting to connect to HeartbeatClient."
        );
        hb = match heartbeat::HeartbeatClient::connect(&provider_for_hb).await {
            Ok(h) => Some(h),
            Err(e) => {
                eprintln!("heartbeat: connect failed: {e:?}");
                return None;
            }
        };
        eprintln!("init_idescriptor_device: Connected to HeartbeatClient.");
    }

    if is_wireless {
        let mut hb_for_task = hb.take().unwrap();
        let udid_for_hb = udid.clone();
        hb_task = Some(RUNTIME.spawn(async move {
            eprintln!("heartbeat: starting heartbeat task ");
            let mut interval = 15u64;
            let mut fails = 0;
            loop {
                eprintln!("heartbeat:  Getting marco (interval: {interval}s)");
                match hb_for_task.get_marco(interval).await {
                    Ok(next) => {
                        interval = next;
                        fails = 0; // Reset failures on success
                        eprintln!("heartbeat:  Received marco, new interval: {interval}s");
                    }
                    Err(e) => {
                        fails += 1;
                        eprintln!("heartbeat:  get_marco failed (fail count: {fails}): {e:?}");
                        if fails >= 3 {
                            eprintln!("heartbeat: too many failures for  giving up");
                            APP_DEVICE_STATE.lock().await.remove(&udid_for_hb);

                            let udid_for_event = udid_for_hb.clone();
                            let _ = qt_thread_for_hb.queue(move |Core_qobj| {
                                Core_qobj.device_event(
                                    2,
                                    &QString::from(udid_for_event),
                                    &QString::from(""),
                                );
                            });
                            break;
                        }
                        continue;
                    }
                }

                eprintln!("heartbeat:  Sending polo.");
                if let Err(e) = hb_for_task.send_polo().await {
                    fails += 1;
                    eprintln!("heartbeat:  send_polo failed (fail count: {fails}): {e:?}");
                    if fails >= 3 {
                        eprintln!("heartbeat: too many failures for , giving up");
                        APP_DEVICE_STATE.lock().await.remove(&udid_for_hb);

                        let udid_for_event = udid_for_hb.clone();
                        let _ = qt_thread_for_hb.queue(move |Core_qobj| {
                            Core_qobj.device_event(
                                2,
                                &QString::from(udid_for_event),
                                &QString::from(""),
                            );
                        });
                        break;
                    }

                    continue;
                }
                eprintln!("heartbeat:  Polo sent successfully.");
                interval += 5;
                // tokio::time::sleep(std::time::Duration::from_secs(interval + 5)).await;
            }

            eprintln!("heartbeat: heartbeat task for  ended.");
            // loop exits → task ends
        }));
    }

    // FIXME: this cannot be done when paired wirelessly
    //    lockdownd_set_value(device->lockdown, "EnableWifiConnections",
    // //                                 value, "com.apple.mobile.wireless_lockdown");

    // let value = Value::Boolean(true);
    // match lc.set_value("EnableWifiConnections", value, Some("com.apple.mobile.wireless_lockdown")).await {
    //     Ok(_) => {},
    //     Err(e) => {
    //         eprintln!("wireless: LockdownClient::set_value failed: {e:?}");
    //         return None;
    //         // continue anyway, as this might not be critical
    //     }
    // }

    let disk_vals = match lc.get_value(None, Some("com.apple.disk_usage")).await {
        Ok(v) => v,
        Err(e) => {
            eprintln!("wireless: get_value(com.apple.disk_usage) failed: {e:?}");
            return None;
        }
    };

    eprintln!(
        "init_idescriptor_device: Attempting to connect to AFC client."
    );
    let mut afc_client = match AfcClient::connect(&provider).await {
        Ok(c) => c,
        Err(e) => {
            eprintln!("AfcClient::connect failed: {e:?}");
            return None;
        }
    };
    eprintln!("init_idescriptor_device: Connected to AfcClient.");

    eprintln!(
        "init_idescriptor_device: Attempting to connect to DiagnosticsRelayClient."
    );
    let mut diag_relay = match DiagnosticsRelayClient::connect(&provider).await {
        Ok(c) => c,
        Err(e) => {
            eprintln!("DiagnosticsRelayClient::connect failed: {e:?}");
            return None;
        }
    };
    eprintln!("init_idescriptor_device: Connected to DiagnosticsRelayClient.");
    // afc_client.set_timeout(Some(5000)).await;

    let afc2 = match   AfcClient::new_afc2(&provider).await {
        Ok(c) => Some(Arc::new(Mutex::new(c))),
        Err(e) => {
            eprintln!("AfcClient::new_afc2 failed: {e:?}");
            None
        }
    };

    eprintln!("init_idescriptor_device: Attempting to get AFC device info.");
    let afc_info = match afc_client.get_device_info().await {
        Ok(i) => i,
        Err(e) => {
            eprintln!("get_device_info failed: {e:?}");
            return None;
        }
    };
    eprintln!("init_idescriptor_device: AFC device info obtained.");

    if let (Value::Dictionary(d_target), Value::Dictionary(d_source)) = (&mut def_vals, disk_vals) {
        d_target.extend(d_source);

        let mut afc_info_dict = plist::Dictionary::new();
        afc_info_dict.insert("Model".into(), Value::String(afc_info.model));
        afc_info_dict.insert(
            "TotalBytes".into(),
            Value::Integer((afc_info.total_bytes as u64).into()),
        );
        afc_info_dict.insert(
            "FreeBytes".into(),
            Value::Integer((afc_info.free_bytes as u64).into()),
        );
        afc_info_dict.insert(
            "BlockSize".into(),
            Value::Integer((afc_info.block_size as u64).into()),
        );

        d_target.insert("AFC_INFO".into(), Value::Dictionary(afc_info_dict));
        d_target.insert(
            "Jailbroken".into(),
            Value::Boolean(utils::detect_jailbroken(&mut afc_client).await),
        );

        if let Some(battery_info) = utils::get_battery_info(&mut diag_relay).await {
            d_target.insert("DIAG_INFO".into(), Value::Dictionary(battery_info));
        }

        d_target.insert(
            "ConnectionType".into(),
            Value::String(if is_wireless {
                "Wireless".into()
            } else {
                "USB".into()
            }),
        );
    }

    //Store device services in AppDeviceState
    eprintln!("init_idescriptor_device: Storing device services.");
    let device_services = DeviceServices {
        afc: Arc::new(Mutex::new(afc_client)),
        afc2,
        diag: Arc::new(Mutex::new(diag_relay)),
        heartbeat_task: hb_task,
        video_streams: Arc::new(Mutex::new(HashMap::new())),
        provider: Arc::new(Mutex::new(Box::new(provider))),
        lockdown: Arc::new(Mutex::new(lc)),
    };

    APP_DEVICE_STATE
        .lock()
        .await
        .insert(udid.to_string(), device_services);

    let mut buf = Vec::new();
    if def_vals.to_writer_xml(&mut buf).is_err() {
        eprintln!(
            "init_idescriptor_device: Failed to serialize default values to XML."
        );
        return None;
    }
    let info = String::from_utf8(buf).ok()?;
    eprintln!("init_idescriptor_device: Device has been initialized.");

    Some((udid, info))
}
