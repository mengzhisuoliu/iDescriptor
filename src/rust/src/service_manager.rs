use cxx_qt::Threading;
use cxx_qt_lib::{
    QByteArray, QList, QMap, QMapPair_QString_QVariant, QString, QVariant,
};
use idevice::{
    IdeviceService, RsdService, amfi, dvt::{
        location_simulation::LocationSimulationClient, remote_server::RemoteServerClient
    }, installation_proxy::InstallationProxyClient, mobile_image_mounter::ImageMounter, provider::IdeviceProvider, rsd::RsdHandshake, simulate_location::LocationSimulationService, springboardservices::SpringBoardServicesClient,
};
use idevice::afc::opcode::AfcFopenMode;
use idevice::services::core_device_proxy::CoreDeviceProxy;
use crate::{APP_DEVICE_STATE, RUNTIME, utils};
use plist::{ Value};

use serde_json;
use std::{
    io::{Read},
    pin::Pin,
    time::Duration,
};
use plist_macro::plist;


#[cxx_qt::bridge(namespace = "CXX")]
mod qobject {
    #[namespace = ""]
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        include!("cxx-qt-lib/qlist.h");
        include!("cxx-qt-lib/qbytearray.h");
        include!("cxx-qt-lib/qmap.h");

        type QString = cxx_qt_lib::QString;
        type QList_QString = cxx_qt_lib::QList<QString>;
        type QByteArray = cxx_qt_lib::QByteArray;
        type QMap_QString_QVariant = cxx_qt_lib::QMap<cxx_qt_lib::QMapPair_QString_QVariant>;
    }

    extern "RustQt" {
        #[qobject]
        type ServiceManager = super::RServiceManager;

        #[qinvokable]
        fn set_udid(self: Pin<&mut ServiceManager>, udid: &QString);

        #[qinvokable]
        fn get_cable_info(&self);

        #[qinvokable]
        fn reveal_developer_mode_option_in_ui(&self);

        #[qinvokable]
        fn query_mobilegestalt(&self, keys: &QList_QString);

        #[qinvokable]
        fn mount_dev_image(&self, image_path: &QString, sig: &QString);

        #[qinvokable]
        fn get_mounted_image(&self);

        #[qinvokable]
        fn fetch_installed_apps(&self);

        #[qinvokable]
        fn set_location(&self, latitude: &QString, longitude: &QString) -> i32;


        #[qinvokable]
        fn fetch_app_icon(&self, bundle_id: QString);

        #[qsignal]
        fn cable_info_retrieved(self: Pin<&mut ServiceManager>, info: QString);

        #[qsignal]
        fn mobilegestalt_info_retrieved(self: Pin<&mut ServiceManager>, info: QMap_QString_QVariant);

        #[qsignal]
        fn dev_image_mounted(self: Pin<&mut ServiceManager>, success: bool);

        #[qsignal]
        fn developer_mode_option_revealed(self: Pin<&mut ServiceManager>, success: bool);

        #[qsignal]
        fn mounted_image_retrieved(self: Pin<&mut ServiceManager>, sig: QByteArray, sig_length: u64);

        #[qsignal]
        fn installed_apps_retrieved(self: Pin<&mut ServiceManager>, apps: &QMap_QString_QVariant);

        #[qsignal]
        fn app_icon_loaded(self: Pin<&mut ServiceManager>, bundle_id: QString, icon_data: QByteArray);


        #[qsignal]
        fn battery_info_updated(self: Pin<&mut ServiceManager>, info: &QString);


        #[qinvokable]
        fn fetch_disk_usage(&self, skip_gallery_usage: bool);

        #[qsignal]
        fn disk_usage_retrieved(self: Pin<&mut ServiceManager>, success: bool, apps_usage: u64, gallery_usage: u64);
    }

    impl cxx_qt::Threading for ServiceManager {}
    impl cxx_qt::Constructor<(QString, u32), NewArguments = (QString, u32)> for ServiceManager {}
}

#[derive(Default)]
pub struct RServiceManager {
    udid: QString,
    ios_version: u32,
}

impl cxx_qt::Constructor<(QString, u32)> for qobject::ServiceManager {
    type BaseArguments = ();
    type InitializeArguments = ();
    type NewArguments = (QString, u32);

    fn route_arguments(
        args: (QString, u32),
    ) -> (
        Self::NewArguments,
        Self::BaseArguments,
        Self::InitializeArguments,
    ) {
        (args, (), ())
    }

    fn new(args: (QString, u32)) -> RServiceManager {
        RServiceManager {
            udid: args.0,
            ios_version: args.1,
        } 
    }

     fn initialize(self: Pin<&mut Self>, _arguments: Self::InitializeArguments) {  
        let udid_for_log = self.get_udid().to_string();  
        println!("ServiceServiceManager::ServiceManager::initialize called for UDID: {udid_for_log}");  
        self.start_update_battery_info_interval();  
    }  
}

impl qobject::ServiceManager {

    pub fn start_update_battery_info_interval(self: Pin<&mut Self>) {  
        let qt_thread = self.qt_thread();  
        let udid = self.get_udid().to_string();
        println!("Starting battery info update interval for device {udid}");
        RUNTIME.spawn(async move {  
            let mut interval = tokio::time::interval(Duration::from_secs(30));  
              
            loop {  
                interval.tick().await;  
                
                let maybe_device = APP_DEVICE_STATE.lock().await.get(udid.as_str()).cloned();

                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        println!("start_update_battery_info_interval: Device {udid} not found");
                        return;
                    }
                };

                println!("start_update_battery_info_interval: Fetching battery info for device {udid}");

                utils::get_battery_info(&mut *device.diag.lock().await).await.map(|info| {
                    let mut buf = Vec::new();
                    if Value::Dictionary(info).to_writer_xml(&mut buf).is_ok() {
                        if let Ok(s) = String::from_utf8(buf) {
                            qt_thread.queue(move |t| {
                                t.battery_info_updated(&QString::from(s));
                            }).ok();
                        }
                    }
                });
            }  
        });  
    }  

    fn get_udid(&self) -> &QString {
        use cxx_qt::CxxQtType;
        &self.rust().udid
    }

    fn get_ios_version(&self) -> u32 {
        use cxx_qt::CxxQtType;
        self.rust().ios_version
    }

    fn set_udid(mut self: Pin<&mut Self>, udid: &QString) {
        use cxx_qt::CxxQtType;
        self.as_mut().rust_mut().udid = udid.clone();
    }

    fn query_mobilegestalt(&self, keys: &QList<QString>) {
        let udid = self.get_udid().to_string();
        let qt_t = self.qt_thread();
        let keys_owned = keys.clone();
        RUNTIME.spawn(async move {
            let mut map = QMap::<QMapPair_QString_QVariant>::default();

            let keys_vec: Vec<String> = keys_owned
                .into_iter()
                .map(|qstr| qstr.to_string())
                .collect();
            let qt_thread = qt_t.clone();
            let maybe_device = APP_DEVICE_STATE.lock().await.get(udid.as_str()).cloned();

            let device = match maybe_device {
                Some(d) => d,
                None => {
                    eprintln!("query_mobilegestalt: device {udid} not found");
                    let _ = qt_thread.queue(move |t| {
                        t.mobilegestalt_info_retrieved(map);
                    });
                    return;
                }
            };

            let result = device.diag.lock().await.mobilegestalt(Some(keys_vec)).await;

            match result {
                Ok(opt_dict) => {
                    if let Some(mut root_dict) = opt_dict {
                         let mobilegestalt_value = root_dict.remove("MobileGestalt");

                         let inner_mobilegestalt_dict = match mobilegestalt_value {
                            Some(Value::Dictionary(dict)) => dict, 
                            _ => {
                                eprintln!(
                                    "query_mobilegestalt: MobileGestalt key not found or not a dictionary for device {udid}."
                                );
                                let _ = qt_thread.queue(move |t| {
                                    t.mobilegestalt_info_retrieved(map);
                                });
                                return;
                            }
                        };

                        for (k, v) in inner_mobilegestalt_dict.into_iter() {
                            let v_str = format!("{v:?}");
                            map.insert(QString::from(k), QVariant::from(&QString::from(v_str)));
                        }
                    }
                    let _ = qt_thread.queue(move |t| {
                        t.mobilegestalt_info_retrieved(map);
                    });
                }
                Err(e) => {
                    eprintln!(
                        "query_mobilegestalt: error querying MobileGestalt for device {udid}: {e}"
                    );
                    let _ = qt_thread.queue(move |t| {
                        t.mobilegestalt_info_retrieved(map);
                    });
                }
            }
        });
    }

    fn get_cable_info(&self) {
        let udid = self.get_udid().to_string();
        let qt_t = self.qt_thread();

        RUNTIME.spawn(async move {
            let qt_thread = qt_t.clone();
            let maybe_device = APP_DEVICE_STATE
                .lock()
                .await
                .get(udid.as_str())
                .cloned();

            let device = match maybe_device {
                Some(d) => d,
                None => {
                    eprintln!("get_cable_info: device {udid} not found");
                    let _ = qt_thread.queue(|t| {
                        t.cable_info_retrieved(QString::from(""));
                    });
                    return;
                }
            };

            let res = utils::get_cable_info(&mut *device.diag.lock().await).await;

            match res {
                Some(dict) => {
                    let mut buf = Vec::new();
                    if Value::Dictionary(dict).to_writer_xml(&mut buf).is_err() {
                        eprintln!(
                            "get_cable_info: Failed to serialize ioregistry values to XML for device {udid}."
                        );
                        let _ = qt_thread.queue(|t| {
                            t.cable_info_retrieved(QString::from(""));
                        });
                        return;
                    }

                    match String::from_utf8(buf) {
                        Ok(s) => {
                            let _ = qt_thread.queue(move |t| {
                                t.cable_info_retrieved(QString::from(s));
                            });
                        }
                        Err(_) => {
                            eprintln!(
                                "get_cable_info: Failed to convert ioregistry XML data to string for device {udid}."
                            );
                            let _ = qt_thread.queue(|t| {
                                t.cable_info_retrieved(QString::from(""));
                            });
                        }
                    }
                }
                None => {
                    eprintln!("get_cable_info: Failed to get ioregistry for device {udid}");
                    let _ = qt_thread.queue(|t| {
                        t.cable_info_retrieved(QString::from(""));
                    });
                }
            }
        });
    }
    fn mount_dev_image(&self, image_path: &QString, sig: &QString) {
        let udid = self.get_udid().to_string();
        let qt_t = self.qt_thread();
        let image = image_path.to_string();
        let signature = sig.to_string();

        RUNTIME.spawn(async move {
            let qt_thread = qt_t.clone();
            let maybe_device = APP_DEVICE_STATE
                .lock()
                .await
                .get(udid.as_str())
                .cloned();

            let device = match maybe_device {
                Some(d) => d,
                None => {
                    eprintln!("get_cable_info: device {udid} not found");
                    let _ = qt_thread.queue(|t| {
                        t.dev_image_mounted(false);
                    });
                    return;
                }
            };

            let mut mounter = match {
                let provider_guard = device.provider.lock().await;
                let provider_ref: &dyn IdeviceProvider = provider_guard.as_ref();
                ImageMounter::connect(provider_ref).await
            } {
                Ok(m) => m,
                Err(e) => {
                    eprintln!("mount_dev_image: Failed to connect to ImageMounter for device {udid}: {e}");
                    let _ = qt_thread.queue(|t| {
                        t.dev_image_mounted(false);
                    });
                    return;
                }
            };

            let mut file = match std::fs::File::open(&image) {
                Ok(f) => f,
                Err(e) => {
                    eprintln!("mount_dev_image: Failed to open image file {image} for device {udid}: {e}");
                    let _ = qt_thread.queue(|t| {
                        t.dev_image_mounted(false);
                    });
                    return;
                }
            };
            let mut buf = Vec::new();
            if let Err(e) = file.read_to_end(&mut buf) {
                eprintln!("mount_dev_image: Failed to read image file {image} for device {udid}: {e}");
                let _ = qt_thread.queue(|t| {
                    t.dev_image_mounted(false);
                });
                return;
            }

            let mut sig_file = match std::fs::File::open(&signature) {
                Ok(f) => f,
                Err(e) => {
                    eprintln!("mount_dev_image: Failed to open signature file {signature} for device {udid}: {e}");
                    let _ = qt_thread.queue(|t| {
                        t.dev_image_mounted(false);
                    });
                    return;
                }
            };

            let mut sig_buf: Vec<u8> = Vec::new();
            if let Err(e) = sig_file.read_to_end(&mut sig_buf) {
                eprintln!("mount_dev_image: Failed to read signature file {signature} for device {udid}: {e}");
                let _ = qt_thread.queue(|t| {
                    t.dev_image_mounted(false);
                });
                return;
            }

            match mounter.mount_developer(&buf, sig_buf).await {
                Ok(_) => {
                    let _ = qt_thread.queue(|t| {
                        t.dev_image_mounted(true);
                    });
                }
                Err(e) => {
                    eprintln!("mount_dev_image: Failed to mount developer image for device {udid}: {e}");
                    let _ = qt_thread.queue(|t| {
                        t.dev_image_mounted(false);
                    });
                }
            };

        });
    }

    fn get_mounted_image(&self) {
        let udid = self.get_udid().to_string();
        let qt_t = self.qt_thread();

        RUNTIME.spawn(async move {
            let qt_thread = qt_t.clone();
            let maybe_device = APP_DEVICE_STATE
                .lock()
                .await
                .get(udid.as_str())
                .cloned();

            let device = match maybe_device {
                Some(d) => d,
                None => {
                    eprintln!("get_cable_info: device {udid} not found");
                    let _ = qt_thread.queue(|t| {
                        t.mounted_image_retrieved(QByteArray::default(), 0);
                    });
                    return;
                }
            };

            let mut mounter = match {
                let provider_guard = device.provider.lock().await;
                let provider_ref: &dyn IdeviceProvider = provider_guard.as_ref();
                ImageMounter::connect(provider_ref).await
            } {
                Ok(m) => m,
                Err(e) => {
                    eprintln!("mount_dev_image: Failed to connect to ImageMounter for device {udid}: {e}");
                    let _ = qt_thread.queue(|t| {
                        t.mounted_image_retrieved(QByteArray::default(), 0);
                    });
                    return;
                }
            };

            match mounter.lookup_image("Developer").await {
                Ok(res) => {
                    let _  = qt_thread.queue(move|t| {
                        t.mounted_image_retrieved(QByteArray::from(&res[..]), res.len() as u64);
                    });
                }
                Err(e) => {
                    eprintln!("get_mounted_image: Failed to lookup mounted developer image for device {udid}: {e}");
                    let _ = qt_thread.queue(|t| {
                        t.mounted_image_retrieved(QByteArray::default(), 0);
                    });
                }
            };

        });
    }

    fn reveal_developer_mode_option_in_ui(&self) {
        let udid = self.get_udid().to_string();
        let qt_t = self.qt_thread();

        RUNTIME.spawn(async move {
            let qt_thread = qt_t.clone();
            let maybe_device = APP_DEVICE_STATE
                .lock()
                .await
                .get(udid.as_str())
                .cloned();

            let device = match maybe_device {
                Some(d) => d,
                None => {
                    eprintln!("get_cable_info: device {udid} not found");
                    let _ = qt_thread.queue(|t| {
                        t.developer_mode_option_revealed(false);
                    });
                    return;
                }
            };


            let mut amfi_client = match {
                let provider_guard = device.provider.lock().await;
                let provider_ref: &dyn IdeviceProvider = provider_guard.as_ref();
                amfi::AmfiClient::connect(provider_ref).await
            } {
                Ok(c) => c,
                Err(e) => {
                    eprintln!("reveal_developer_mode_option_in_ui: Failed to connect to AMFI service for device {udid}: {e}");
                    let _ = qt_thread.queue(|t| {
                        t.developer_mode_option_revealed(false);
                    });
                    return;
                }
            };


            match amfi_client.reveal_developer_mode_option_in_ui().await {
                Ok(_) => {
                    let _ = qt_thread.queue(|t| {
                        t.developer_mode_option_revealed(true);
                    });
                }
                Err(e) => {
                    eprintln!("reveal_developer_mode_option_in_ui: Failed to reveal developer mode option in UI for device {udid}: {e}");
                    let _ = qt_thread.queue(|t| {
                        t.developer_mode_option_revealed(false);
                    });
            }

            }
        });
    }

    fn fetch_installed_apps(&self) {
        let udid = self.get_udid().to_string();
        let qt_t = self.qt_thread();

        RUNTIME.spawn(async move {
            let qt_thread = qt_t.clone();
            let maybe_device = APP_DEVICE_STATE
                .lock()
                .await
                .get(udid.as_str())
                .cloned();

            let mut all_apps = QMap::<QMapPair_QString_QVariant>::default();

            let device = match maybe_device {
                Some(d) => d,
                None => {
                    eprintln!("fetch_installed_apps: device {udid} not found");
                    let _ = qt_thread.queue(move |t| {
                        t.installed_apps_retrieved(&all_apps);
                    });
                    return;
                }
            };

            let mut ins_client = match {
                let provider_guard = device.provider.lock().await;
                let provider_ref: &dyn IdeviceProvider = provider_guard.as_ref();
                InstallationProxyClient::connect(provider_ref).await
            } {
                Ok(c) => c,
                Err(e) => {
                    eprintln!("fetch_installed_apps: Failed to connect to InstallationProxy service for device {udid}: {e}");
                    let _ = qt_thread.queue( move |t| {
                        t.installed_apps_retrieved(&all_apps);
                    });
                    return;
                }
            };

            // Get both User and System apps
            for app_type in ["User", "System"] {
                let client_options = plist!({
                    "ApplicationType": app_type,
                    "ReturnAttributes": [
                        "CFBundleIdentifier",
                        "CFBundleDisplayName",
                        "CFBundleShortVersionString",
                        "CFBundleVersion",
                        "UIFileSharingEnabled"
                    ]
                });

                let apps = match ins_client.browse(Some(client_options)).await {
                    Ok(apps) => apps,
                    Err(e) => {
                        eprintln!("fetch_installed_apps: Failed to browse installed apps for device {udid} and app type {app_type}: {e}");
                        continue; // Skip this app type 
                    }
                };

                for app_info in apps {
                    if let plist::Value::Dictionary(app_dict) = app_info {
                        let bundle_id = app_dict
                            .get("CFBundleIdentifier")
                            .and_then(|v| v.as_string())
                            .unwrap_or_default();

                        if bundle_id.is_empty() {
                            continue;
                        }

                        let display = app_dict
                            .get("CFBundleDisplayName")
                            .and_then(|v| v.as_string())
                            .unwrap_or_default();
                        let version = app_dict
                            .get("CFBundleShortVersionString")
                            .and_then(|v| v.as_string())
                            .unwrap_or_default();
                        let fs_enabled = app_dict
                            .get("UIFileSharingEnabled")
                            .and_then(|v| v.as_boolean())
                            .unwrap_or(false);

                        let app_json = format!(
                            "{{\"bundle_id\":{},\"CFBundleDisplayName\":{},\"CFBundleShortVersionString\":{},\"UIFileSharingEnabled\":{},\"app_type\":{}}}",
                            serde_json::to_string(&bundle_id).unwrap_or_else(|_| format!("\"{}\"", bundle_id)),
                            serde_json::to_string(&display).unwrap_or_else(|_| format!("\"{}\"", display)),
                            serde_json::to_string(&version).unwrap_or_else(|_| format!("\"{}\"", version)),
                            fs_enabled,
                            serde_json::to_string(&app_type).unwrap_or_else(|_| format!("\"{}\"", app_type)),
                        );

                        all_apps.insert(
                            QString::from(bundle_id),
                            QVariant::from(&QString::from(app_json)),
                        );
                    }
                }
            }

            let _ = qt_thread.queue(move |t| {
                t.installed_apps_retrieved(&all_apps);
            });
        });
    }


    fn fetch_app_icon(&self, bundle_id: QString) {
        let udid = self.get_udid().to_string();
        let qt_t = self.qt_thread();
        let bundle_id_owned = bundle_id.clone();

        RUNTIME.spawn(async move {
            let maybe_device = APP_DEVICE_STATE
                .lock()
                .await
                .get(udid.as_str())
                .cloned();

            let device = match maybe_device {
                Some(d) => d,
                None => {
                    eprintln!("fetch_app_icon: device {udid} not found");
                    return;
                }
            };

            let mut springboard_client = match {
                let provider_guard = device.provider.lock().await;
                let provider_ref: &dyn IdeviceProvider = provider_guard.as_ref();
                SpringBoardServicesClient::connect(provider_ref).await
            } {
                Ok(c) => c,
                Err(e) => {
                    eprintln!("fetch_app_icon: Failed to connect to InstallationProxy service for device {udid}: {e}");
                    return;
                }
            };

            match springboard_client.get_icon_pngdata(bundle_id_owned.to_string()).await {
                Ok(icon_data) => {
                    qt_t.queue(move |t| {
                        t.app_icon_loaded(bundle_id_owned, QByteArray::from(&icon_data[..]));
                    }).ok();
                }
                Err(e) => {
                    eprintln!("fetch_app_icon: Failed to fetch app icon for bundle ID {} on device {udid}: {e}", bundle_id_owned);
                }
            };
        });
    }


    fn set_location(&self, latitude: &QString, longitude: &QString) -> i32 {
        let udid = self.get_udid().to_string();
        let ios_version = self.get_ios_version();
        
        /*
            FIXME: use RUNTIME.spawn in the future
        */
        RUNTIME.block_on(async move {
            tokio::select! {
                res = async {
                    let maybe_device = APP_DEVICE_STATE
                        .lock()
                        .await
                        .get(udid.as_str())
                        .cloned();

                    let device = match maybe_device {
                        Some(d) => d,
                        None => {
                            eprintln!("set_location: device {udid} not found");
                            return -1;
                        }
                    };

                    let result = {
                        let provider_guard = device.provider.lock().await;

                        if ios_version < 17 {
                            set_device_location_lockdown( provider_guard.as_ref(), latitude.to_string().as_str(), longitude.to_string().as_str()).await
                        } else {
                            println!("Using RSD path for setting location on device {udid} with iOS version {ios_version}");
                            let proxy_res = {
                                CoreDeviceProxy::connect(provider_guard.as_ref()).await
                            };

                            match proxy_res {
                                Ok(proxy) => set_device_location_rsd(proxy, utils::qstring_to_f64(latitude).unwrap_or(0.0), utils::qstring_to_f64(longitude).unwrap_or(0.0)).await,
                                Err(err) => {
                                    println!("Failed to connect to CoreDeviceProxy for device {udid}, cannot set location using RSD path.");
                                    return err.code();
                                },
                            }
                        }
                    };

                    match result {
                        Ok(_) => 0,
                        Err(e) => {
                            eprintln!(
                                "set_location: failed to set virtual location for device {udid}: {e:?}"
                            );
                            return e.code();
                        }
                    }
                } => {
                    res
                }
                _ = tokio::time::sleep(Duration::from_secs(10)) => {
                    eprintln!("set_location: timed out");
                    -71
                }
            }
        })
    }

    fn fetch_disk_usage(&self, skip_gallery_usage: bool) {
        let udid = self.get_udid().to_string();
        let qt_t = self.qt_thread();

        RUNTIME.spawn(async move {
            let qt_thread = qt_t.clone();

            let mut instproxy = {                
                let maybe_device = APP_DEVICE_STATE
                    .lock()
                    .await
                    .get(udid.as_str())
                    .cloned();
    
                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("fetch_disk_usage: device {udid} not found");
                        return;
                    }
                };


                match InstallationProxyClient::connect(device.provider.lock().await.as_ref()).await {
                    Ok(c) => c,
                    Err(e) => {
                        eprintln!("fetch_disk_usage: Failed to connect to InstallationProxy service for device {udid}: {e}");
                        qt_thread.queue(move |t| {
                            t.disk_usage_retrieved(false, 0, 0);
                        }).ok();
                        return;
                    }
                }
            };
            

            match utils::calculate_apps_usage(&mut instproxy).await {
                Ok(apps_usage) => {
                    
                    if (skip_gallery_usage) {
                        qt_thread.queue(move |t| {
                            t.disk_usage_retrieved(true, apps_usage, 0);
                        }).ok();
                        return;
                    }
                    let maybe_device = APP_DEVICE_STATE
                    .lock()
                    .await
                    .get(udid.as_str())
                    .cloned();
    
                    let device = match maybe_device {
                        Some(d) => d,
                        None => {
                            eprintln!("fetch_disk_usage: device {udid} not found");
                            return;
                        }
                    };

                    let mut afc = device.afc.lock().await;

                    // FIXME: go safer here
                    let mut fd = afc.open("/PhotoData/Photos.sqlite", AfcFopenMode::RdOnly).await.map_err(|e| {
                        eprintln!("fetch_disk_usage: Failed to read gallery database for device {udid}: {e}");
                        e
                     }).unwrap();

                    let mut gallery_db_bytes = fd.read_entire().await.unwrap();


                    match utils::query_gallery_usage(&mut gallery_db_bytes) {
                        Ok(gallery_usage) => {
                            qt_thread.queue(move |t| {
                                t.disk_usage_retrieved(true, apps_usage, gallery_usage);
                            }).ok();
                        }
                        Err(e) => {
                            eprintln!("fetch_disk_usage: Failed to calculate gallery disk usage for device {udid}: {e}");
                            qt_thread.queue(move |t| {
                                t.disk_usage_retrieved(true, apps_usage, 0);
                            }).ok();
                        }
                    }

                    
                }
                Err(e) => {
                    eprintln!("fetch_disk_usage: Failed to calculate apps disk usage for device {udid}: {e}");
                    qt_thread.queue(move |t| {
                        t.disk_usage_retrieved(false, 0, 0);
                    }).ok();
                }
            };
        

        });
    }
}

async fn set_device_location_lockdown(
    provider: &dyn IdeviceProvider,
    latitude: &str,
    longitude: &str,
) -> Result<(), idevice::IdeviceError> {
    // iOS 16 and below: use lockdown-based LocationSimulation service
    let mut client = LocationSimulationService::connect(provider).await?;
    client.set(latitude, longitude).await
}

// iOS 17+:
async fn set_device_location_rsd(
    proxy: CoreDeviceProxy,
    latitude: f64,
    longitude: f64,
) -> Result<(), idevice::IdeviceError> {
    let rsd_port = proxy.handshake.server_rsd_port;
    let adapter = proxy.create_software_tunnel()?;  
    let mut adapter = adapter.to_async_handle();  
    let stream = adapter.connect(rsd_port).await?;  
  
    let mut handshake = RsdHandshake::new(stream).await?;  
  
    let mut remote_server =  
        RemoteServerClient::connect_rsd(&mut adapter, &mut handshake).await?;  
    remote_server.read_message(0).await?;  
  
    let mut location_client = LocationSimulationClient::new(&mut remote_server).await?;  
    location_client.set(latitude, longitude).await  
}
