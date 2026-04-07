use crate::{APP_DEVICE_STATE, RUNTIME, run_sync, utils};
use cxx_qt::{CxxQtType, Threading};
use cxx_qt_lib::QUuid;
use idevice::{IdeviceService, afc::AfcClient, services::afc::opcode::AfcFopenMode};
use std::{
    collections::HashMap,
    path::{Path, PathBuf},
    pin::Pin,
    sync::{
        Arc, Mutex,
        atomic::{AtomicBool, Ordering},
    },
};
use tokio::{fs, io::AsyncWriteExt};

#[cxx_qt::bridge(namespace = "CXX")]
mod qobject {
    #[namespace = ""]
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        include!("cxx-qt-lib/qlist.h");
        include!("cxx-qt-lib/quuid.h");

        type QString = cxx_qt_lib::QString;
        type QList_QString = cxx_qt_lib::QList<cxx_qt_lib::QString>;
        type QUuid = cxx_qt_lib::QUuid;
    }

    extern "RustQt" {
        #[qobject]
        type IOManager = super::RIOManager;

        #[qinvokable]
        fn start_export(
            self: Pin<&mut IOManager>,
            udid: &QString,
            job_id: &QUuid,
            device_paths: &QList_QString,
            destination_dir: &QString,
        );

        #[qinvokable]
        fn start_export_with_afc2(
            self: Pin<&mut IOManager>,
            udid: &QString,
            job_id: &QUuid,
            device_paths: &QList_QString,
            destination_dir: &QString,
        );

        #[qinvokable]
        fn start_export_with_hause_arrest_afc(
            self: Pin<&mut IOManager>,
            udid: &QString,
            job_id: &QUuid,
            device_paths: &QList_QString,
            destination_dir: &QString,
            hause_arrest_afc: &QString,
        );

        #[qinvokable]
        fn start_import(
            self: Pin<&mut IOManager>,
            udid: &QString,
            job_id: &QUuid,
            local_paths: &QList_QString,
            destination_dir: &QString,
        );

        #[qinvokable]
        fn start_import_with_afc2(
            self: Pin<&mut IOManager>,
            udid: &QString,
            job_id: &QUuid,
            local_paths: &QList_QString,
            destination_dir: &QString,
        );

        #[qinvokable]
        fn start_import_with_hause_arrest_afc(
            self: Pin<&mut IOManager>,
            udid: &QString,
            job_id: &QUuid,
            local_paths: &QList_QString,
            destination_dir: &QString,
            hause_arrest_afc: &QString,
        );

        #[qinvokable]
        fn cancel_job(self: Pin<&mut IOManager>, job_id: &QUuid);

        #[qinvokable]
        fn cancel_all_jobs(self: Pin<&mut IOManager>);

        #[qsignal]
        fn file_transfer_progress(
            self: Pin<&mut IOManager>,
            job_id: &QUuid,
            file_name: &QString,
            bytes_transferred: i64,
            total_bytes: i64,
        );

        #[qsignal]
        fn export_item_finished(
            self: Pin<&mut IOManager>,
            job_id: &QUuid,
            file_name: &QString,
            destination_path: &QString,
            success: bool,
            bytes_transferred: i64,
            error_message: &QString,
        );

        #[qsignal]
        fn export_job_finished(
            self: Pin<&mut IOManager>,
            job_id: &QUuid,
            cancelled: bool,
            successful_items: i32,
            failed_items: i32,
            total_bytes: i64,
        );

        #[qsignal]
        fn import_item_finished(
            self: Pin<&mut IOManager>,
            job_id: &QUuid,
            file_name: &QString,
            destination_path: &QString,
            success: bool,
            bytes_transferred: i64,
            error_message: &QString,
        );

        #[qsignal]
        fn import_job_finished(
            self: Pin<&mut IOManager>,
            job_id: &QUuid,
            cancelled: bool,
            successful_items: i32,
            failed_items: i32,
            total_bytes: i64,
        );

        #[qinvokable]
        fn release_video_streamer(self: &IOManager, udid: &QString, url: &QString);
    }

    impl cxx_qt::Threading for IOManager {}
}

struct ExportItemResult {
    success: bool,
    bytes_transferred: i64,
    destination_path: String,
    error_message: Option<String>,
}

#[derive(Default)]
pub struct RIOManager {
    jobs: Arc<Mutex<HashMap<String, Arc<AtomicBool>>>>,
}

impl qobject::IOManager {
    fn start_export(
        self: Pin<&mut Self>,
        udid: &qobject::QString,
        job_id: &qobject::QUuid,
        device_paths: &qobject::QList_QString,
        destination_dir: &qobject::QString,
    ) {
        let udid_str = udid.to_string();
        let dest_dir_str = destination_dir.to_string();

        let mut items = Vec::with_capacity(device_paths.len() as usize);
        for i in 0..device_paths.len() {
            if let Some(p) = device_paths.get(i) {
                items.push(p.to_string());
            }
        }

        let job_id_str = job_id.to_string();

        let cancel_flag = Arc::new(AtomicBool::new(false));
        let jobs_map = self.as_ref().rust().jobs.clone();
        {
            let mut guard = jobs_map.lock().expect("IOManager jobs map mutex poisoned");
            guard.insert(job_id_str.clone(), cancel_flag.clone());
        }

        let qt_thread = self.qt_thread();
        let jobs_map_for_task = jobs_map.clone();
        let job_id_for_task = job_id.clone();
        let items_for_task = items.clone();
        let cancel_flag_for_task = cancel_flag.clone();

        RUNTIME.spawn(async move {
            let mut afc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(&udid_str).cloned();
                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("IOManager: device {udid_str} not found");
                        let _ = qt_thread.queue(move |mgr| {
                            mgr.export_job_finished(&job_id_for_task, true, 0, 0, 0);
                        });
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        return;
                    }
                };
                /*
                    create a new AFC client for this task
                    so we can export as fast as possible
                */
                match AfcClient::connect(device.provider.lock().await.as_ref()).await {
                    Ok(c) => c,
                    Err(e) => {
                        eprintln!("Failed to create AFC2 client: {e}");
                        //FIXME: create failed signal
                        let _ = qt_thread.queue(move |mgr| {
                            mgr.export_job_finished(&job_id_for_task, true, 0, 0, 0);
                        });
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        eprintln!("AfcClient::new_afc2 failed");
                        return;
                    }
                }
            };

            handle_start_export(
                &mut afc,
                &job_id_for_task,
                &items_for_task,
                dest_dir_str,
                &qt_thread,
                &jobs_map_for_task,
                &cancel_flag_for_task,
            )
            .await;
        });
    }

    fn start_export_with_afc2(
        self: Pin<&mut Self>,
        udid: &qobject::QString,
        job_id: &qobject::QUuid,
        device_paths: &qobject::QList_QString,
        destination_dir: &qobject::QString,
    ) {
        let udid_str = udid.to_string();
        let dest_dir_str = destination_dir.to_string();

        let mut items = Vec::with_capacity(device_paths.len() as usize);
        for i in 0..device_paths.len() {
            if let Some(p) = device_paths.get(i) {
                items.push(p.to_string());
            }
        }

        let job_id_str = job_id.to_string();

        let cancel_flag = Arc::new(AtomicBool::new(false));
        let jobs_map = self.as_ref().rust().jobs.clone();
        {
            let mut guard = jobs_map.lock().expect("IOManager jobs map mutex poisoned");
            guard.insert(job_id_str.clone(), cancel_flag.clone());
        }

        let qt_thread = self.qt_thread();
        let jobs_map_for_task = jobs_map.clone();
        let job_id_for_task = job_id.clone();
        let items_for_task = items.clone();
        let cancel_flag_for_task = cancel_flag.clone();

        RUNTIME.spawn(async move {
            /*
                create a new AFC2 client for this task
                so we can export as fast as possible
            */
            let mut afc2 = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(&udid_str).cloned();
                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("IOManager: device {udid_str} not found");
                        let _ = qt_thread.queue(move |mgr| {
                            mgr.export_job_finished(&job_id_for_task, true, 0, 0, 0);
                        });
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        return;
                    }
                };

                match AfcClient::new_afc2(device.provider.lock().await.as_ref()).await {
                    Ok(c) => c,
                    Err(e) => {
                        eprintln!("Failed to create AFC2 client: {e}");
                        let _ = qt_thread.queue(move |mgr| {
                            mgr.export_job_finished(&job_id_for_task, true, 0, 0, 0);
                        });
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        eprintln!("AfcClient::new_afc2 failed");
                        return;
                    }
                }
            };

            handle_start_export(
                &mut afc2,
                &job_id_for_task,
                &items_for_task,
                dest_dir_str,
                &qt_thread,
                &jobs_map_for_task,
                &cancel_flag_for_task,
            )
            .await;
        });
    }

    fn start_export_with_hause_arrest_afc(
        self: Pin<&mut Self>,
        udid: &qobject::QString,
        job_id: &qobject::QUuid,
        device_paths: &qobject::QList_QString,
        destination_dir: &qobject::QString,
        hause_arrest_afc: &qobject::QString,
    ) {
        let udid_str = udid.to_string();
        let dest_dir_str = destination_dir.to_string();

        let mut items = Vec::with_capacity(device_paths.len() as usize);
        for i in 0..device_paths.len() {
            if let Some(p) = device_paths.get(i) {
                items.push(p.to_string());
            }
        }

        let job_id_str = job_id.to_string();

        let cancel_flag = Arc::new(AtomicBool::new(false));
        let jobs_map = self.as_ref().rust().jobs.clone();
        {
            let mut guard = jobs_map.lock().expect("IOManager jobs map mutex poisoned");
            guard.insert(job_id_str.clone(), cancel_flag.clone());
        }

        let qt_thread = self.qt_thread();
        let jobs_map_for_task = jobs_map.clone();
        let job_id_for_task = job_id.clone();
        let items_for_task = items.clone();
        let cancel_flag_for_task = cancel_flag.clone();
        let bundle_id_str = hause_arrest_afc.to_string();

        RUNTIME.spawn(async move {
            /*
                create a new HouseArrest AFC client for this task
                so we can export as fast as possible
            */
            let mut hause_arrest_afc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(&udid_str).cloned();
                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("IOManager: device {udid_str} not found");
                        let _ = qt_thread.queue(move |mgr| {
                            mgr.export_job_finished(&job_id_for_task, true, 0, 0, 0);
                        });
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        return;
                    }
                };

                let provider_guard = device.provider.lock().await;

                match utils::vend_app_documents(provider_guard.as_ref(), &bundle_id_str).await {
                    Ok(afc_client) => afc_client,
                    Err(e) => {
                        eprintln!(
                            "Failed to initialize HouseArrest session for {}: {}",
                            bundle_id_str, e
                        );
                        eprintln!("Failed to create AFC2 client: {e}");
                        let _ = qt_thread.queue(move |mgr| {
                            mgr.export_job_finished(&job_id_for_task, true, 0, 0, 0);
                        });
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        return;
                    }
                }
            };

            handle_start_export(
                &mut hause_arrest_afc,
                &job_id_for_task,
                &items_for_task,
                dest_dir_str,
                &qt_thread,
                &jobs_map_for_task,
                &cancel_flag_for_task,
            )
            .await;
        });
    }

    fn start_import(
        self: Pin<&mut Self>,
        udid: &qobject::QString,
        job_id: &qobject::QUuid,
        local_paths: &qobject::QList_QString,
        destination_dir: &qobject::QString,
    ) {
        let udid_str = udid.to_string();
        let dest_dir_device_str = destination_dir.to_string();

        let mut items = Vec::with_capacity(local_paths.len() as usize);
        for i in 0..local_paths.len() {
            if let Some(p) = local_paths.get(i) {
                items.push(p.to_string());
            }
        }

        let job_id_str = job_id.to_string();

        let cancel_flag = Arc::new(AtomicBool::new(false));
        let jobs_map = self.as_ref().rust().jobs.clone();
        {
            let mut guard = jobs_map.lock().expect("IOManager jobs map mutex poisoned");
            guard.insert(job_id_str.clone(), cancel_flag.clone());
        }

        let qt_thread = self.qt_thread();
        let jobs_map_for_task = jobs_map.clone();
        let job_id_for_task = job_id.clone();
        let items_for_task = items.clone();
        let cancel_flag_for_task = cancel_flag.clone();

        RUNTIME.spawn(async move {
            let mut afc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(&udid_str).cloned();
                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("IOManager (import): device {udid_str} not found");
                        let job_id_signal = job_id_for_task.clone();
                        qt_thread
                            .queue(move |mgr| {
                                mgr.import_job_finished(&job_id_signal, true, 0, 0, 0);
                            })
                            .ok();
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        return;
                    }
                };

                match AfcClient::connect(device.provider.lock().await.as_ref()).await {
                    Ok(c) => c,
                    Err(e) => {
                        eprintln!("Failed to create AFC client for import: {e}");
                        let job_id_signal = job_id_for_task.clone();
                        qt_thread
                            .queue(move |mgr| {
                                mgr.import_job_finished(&job_id_signal, true, 0, 0, 0);
                            })
                            .ok();
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        return;
                    }
                }
            };

            handle_start_import(
                &mut afc,
                &job_id_for_task,
                &items_for_task,
                dest_dir_device_str,
                &qt_thread,
                &jobs_map_for_task,
                &cancel_flag_for_task,
            )
            .await;
        });
    }

    fn start_import_with_afc2(
        self: Pin<&mut Self>,
        udid: &qobject::QString,
        job_id: &qobject::QUuid,
        local_paths: &qobject::QList_QString,
        destination_dir: &qobject::QString,
    ) {
        let udid_str = udid.to_string();
        let dest_dir_device_str = destination_dir.to_string();

        let mut items = Vec::with_capacity(local_paths.len() as usize);
        for i in 0..local_paths.len() {
            if let Some(p) = local_paths.get(i) {
                items.push(p.to_string());
            }
        }

        let job_id_str = job_id.to_string();

        let cancel_flag = Arc::new(AtomicBool::new(false));
        let jobs_map = self.as_ref().rust().jobs.clone();
        {
            let mut guard = jobs_map.lock().expect("IOManager jobs map mutex poisoned");
            guard.insert(job_id_str.clone(), cancel_flag.clone());
        }

        let qt_thread = self.qt_thread();
        let jobs_map_for_task = jobs_map.clone();
        let job_id_for_task = job_id.clone();
        let items_for_task = items.clone();
        let cancel_flag_for_task = cancel_flag.clone();

        RUNTIME.spawn(async move {
            let mut afc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(&udid_str).cloned();
                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("IOManager (import): device {udid_str} not found");
                        let job_id_signal = job_id_for_task.clone();
                        qt_thread
                            .queue(move |mgr| {
                                mgr.import_job_finished(&job_id_signal, true, 0, 0, 0);
                            })
                            .ok();
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        return;
                    }
                };
                match AfcClient::new_afc2(device.provider.lock().await.as_ref()).await {
                    Ok(c) => c,
                    Err(e) => {
                        eprintln!("Failed to create AFC2 client for import: {e}");
                        let job_id_signal = job_id_for_task.clone();
                        qt_thread
                            .queue(move |mgr| {
                                mgr.import_job_finished(&job_id_signal, true, 0, 0, 0);
                            })
                            .ok();
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        return;
                    }
                }
            };

            handle_start_import(
                &mut afc,
                &job_id_for_task,
                &items_for_task,
                dest_dir_device_str,
                &qt_thread,
                &jobs_map_for_task,
                &cancel_flag_for_task,
            )
            .await;
        });
    }

    fn start_import_with_hause_arrest_afc(
        self: Pin<&mut Self>,
        udid: &qobject::QString,
        job_id: &qobject::QUuid,
        local_paths: &qobject::QList_QString,
        destination_dir: &qobject::QString,
        hause_arrest_afc: &qobject::QString,
    ) {
        let udid_str = udid.to_string();
        let dest_dir_device_str = destination_dir.to_string();

        let mut items = Vec::with_capacity(local_paths.len() as usize);
        for i in 0..local_paths.len() {
            if let Some(p) = local_paths.get(i) {
                items.push(p.to_string());
            }
        }

        let job_id_str = job_id.to_string();

        let cancel_flag = Arc::new(AtomicBool::new(false));
        let jobs_map = self.as_ref().rust().jobs.clone();
        {
            let mut guard = jobs_map.lock().expect("IOManager jobs map mutex poisoned");
            guard.insert(job_id_str.clone(), cancel_flag.clone());
        }

        let qt_thread = self.qt_thread();
        let jobs_map_for_task = jobs_map.clone();
        let job_id_for_task = job_id.clone();
        let items_for_task = items.clone();
        let cancel_flag_for_task = cancel_flag.clone();
        let bundle_id_str = hause_arrest_afc.to_string();

        RUNTIME.spawn(async move {
            let mut hause_arrest_afc = {
                let maybe_device = APP_DEVICE_STATE.lock().await.get(&udid_str).cloned();
                let device = match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("IOManager: device {udid_str} not found");
                        let _ = qt_thread.queue(move |mgr| {
                            mgr.export_job_finished(&job_id_for_task, true, 0, 0, 0);
                        });
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        return;
                    }
                };

                let provider_guard = device.provider.lock().await;

                match utils::vend_app_documents(provider_guard.as_ref(), &bundle_id_str).await {
                    Ok(afc_client) => afc_client,
                    Err(e) => {
                        eprintln!(
                            "Failed to initialize HouseArrest session for {}: {}",
                            bundle_id_str, e
                        );
                        eprintln!("Failed to create AFC2 client: {e}");
                        let _ = qt_thread.queue(move |mgr| {
                            mgr.export_job_finished(&job_id_for_task, true, 0, 0, 0);
                        });
                        let mut guard = jobs_map_for_task
                            .lock()
                            .expect("IOManager jobs map mutex poisoned");
                        guard.remove(&job_id_str);
                        return;
                    }
                }
            };

            handle_start_import(
                &mut hause_arrest_afc,
                &job_id_for_task,
                &items_for_task,
                dest_dir_device_str,
                &qt_thread,
                &jobs_map_for_task,
                &cancel_flag_for_task,
            )
            .await;
        });
    }

    fn cancel_job(self: Pin<&mut Self>, job_id: &qobject::QUuid) {
        let job_id_str = job_id.to_string();
        let jobs_map = self.rust_mut().jobs.clone();
        let guard = jobs_map.lock().expect("IOManager jobs map mutex poisoned");
        if let Some(flag) = guard.get(&job_id_str) {
            flag.store(true, Ordering::Relaxed);
        }
    }

    fn cancel_all_jobs(self: Pin<&mut Self>) {
        let jobs_map = self.rust_mut().jobs.clone();
        let guard = jobs_map.lock().expect("IOManager jobs map mutex poisoned");
        for flag in guard.values() {
            flag.store(true, Ordering::Relaxed);
        }
    }

    fn release_video_streamer(&self, udid: &qobject::QString, url: &qobject::QString) {
        let udid_str = udid.to_string();
        let url_str = url.to_string();
        let url_str_clone = url_str.clone();
        let tx_opt = run_sync(async move {
            let mut state = APP_DEVICE_STATE.lock().await;
            let Some(device) = state.get_mut(&udid_str) else {
                eprintln!("release_streamer: device {udid_str} not found");
                return None;
            };

            let mut streams = device.video_streams.lock().await;
            streams.remove(&url_str)
        });

        if let Some(tx) = tx_opt {
            eprintln!("release_streamer: sending shutdown for URL: {url_str_clone}");
            let _ = tx.send(());
        } else {
            eprintln!("release_streamer: no streamer found for URL: {url_str_clone}");
        }
    }
}

async fn handle_start_export(
    afc: &mut AfcClient,
    job_id: &qobject::QUuid,
    device_paths: &Vec<String>,
    destination_dir: String,
    qt_thread: &cxx_qt::CxxQtThread<qobject::IOManager>,
    jobs_map_for_task: &Arc<Mutex<HashMap<String, Arc<AtomicBool>>>>,
    cancel_flag: &Arc<AtomicBool>,
) {
    let mut items = Vec::with_capacity(device_paths.len() as usize);
    for i in 0..device_paths.len() {
        if let Some(p) = device_paths.get(i) {
            items.push(p.to_string());
        }
    }

    let job_id_str = job_id.to_string();
    let job_id_for_task = job_id.clone();

    let mut successful = 0_i32;
    let mut failed = 0_i32;
    let mut total_bytes = 0_i64;
    let mut cancelled = false;

    for device_path in items {
        if cancel_flag.load(Ordering::Relaxed) {
            cancelled = true;
            break;
        }

        let res = export_single_item(
            afc,
            &device_path,
            &destination_dir,
            &job_id_for_task,
            qt_thread,
            cancel_flag,
        )
        .await;

        match res {
            Ok(r) if r.success => {
                successful += 1;
                total_bytes += r.bytes_transferred;
                let file_name = Path::new(&device_path)
                    .file_name()
                    .and_then(|s| s.to_str())
                    .unwrap_or(&device_path)
                    .to_string();
                let job_id_signal = job_id_for_task.clone();
                let dest_clone = r.destination_path.clone();
                qt_thread
                    .queue(move |mgr| {
                        mgr.export_item_finished(
                            &job_id_signal,
                            &qobject::QString::from(file_name),
                            &qobject::QString::from(dest_clone),
                            true,
                            r.bytes_transferred,
                            &qobject::QString::from(""),
                        );
                    })
                    .ok();
            }
            Ok(r) => {
                failed += 1;
                let file_name = Path::new(&device_path)
                    .file_name()
                    .and_then(|s| s.to_str())
                    .unwrap_or(&device_path)
                    .to_string();
                let err_msg = r
                    .error_message
                    .unwrap_or_else(|| "Unknown error".to_string());
                let job_id_signal = job_id_for_task.clone();
                let dest_clone = r.destination_path.clone();
                qt_thread
                    .queue(move |mgr| {
                        mgr.export_item_finished(
                            &job_id_signal,
                            &qobject::QString::from(file_name),
                            &qobject::QString::from(dest_clone),
                            false,
                            r.bytes_transferred,
                            &qobject::QString::from(err_msg),
                        );
                    })
                    .ok();
            }
            Err(err) => {
                failed += 1;
                let file_name = Path::new(&device_path)
                    .file_name()
                    .and_then(|s| s.to_str())
                    .unwrap_or(&device_path)
                    .to_string();
                let job_id_signal = job_id_for_task.clone();
                qt_thread
                    .queue(move |mgr| {
                        mgr.export_item_finished(
                            &job_id_signal,
                            &qobject::QString::from(file_name),
                            &qobject::QString::from(""),
                            false,
                            0,
                            &qobject::QString::from(err),
                        );
                    })
                    .ok();
            }
        }
    }

    let job_id_signal = job_id_for_task.clone();
    qt_thread
        .queue(move |mgr| {
            mgr.export_job_finished(&job_id_signal, cancelled, successful, failed, total_bytes);
        })
        .ok();

    let mut guard = jobs_map_for_task
        .lock()
        .expect("IoManager jobs map mutex poisoned");
    guard.remove(&job_id_str);
}

async fn export_single_item(
    afc_client: &mut AfcClient,
    device_path: &str,
    destination_dir: &str,
    job_id: &QUuid,
    qt_thread: &cxx_qt::CxxQtThread<qobject::IOManager>,
    cancel_flag: &Arc<AtomicBool>,
) -> Result<ExportItemResult, String> {
    use tokio::io::AsyncReadExt;

    // Ensure destination directory exists.
    if let Err(e) = fs::create_dir_all(destination_dir).await {
        return Err(format!(
            "Failed to create destination directory {}: {e}",
            destination_dir
        ));
    }

    let file_name = Path::new(device_path)
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or(device_path);

    let base_path = Path::new(destination_dir).join(file_name);
    let output_path = generate_unique_output_path(&base_path).await;
    let output_path_str = output_path
        .to_str()
        .unwrap_or_else(|| base_path.to_str().unwrap_or(""))
        .to_string();

    // Use AFC get_file_info for size and timestamps
    let info = afc_client
        .get_file_info(device_path.to_string())
        .await
        .map_err(|e| format!("Failed to get file info for {device_path}: {e}"))?;
    let file_size = info.size as i64;
    let modified = info.modified;

    let mut remote = afc_client
        .open(device_path, AfcFopenMode::RdOnly)
        .await
        .map_err(|e| format!("Failed to open device file {device_path}: {e}"))?;

    let mut local = fs::File::create(&output_path)
        .await
        .map_err(|e| format!("Failed to create local file {output_path_str}: {e}"))?;

    let mut buf = [0u8; 8192];
    let mut transferred: i64 = 0;

    loop {
        if cancel_flag.load(Ordering::Relaxed) {
            break;
        }

        let n = remote
            .read(&mut buf)
            .await
            .map_err(|e| format!("Failed to read from device file {device_path}: {e}"))?;
        if n == 0 {
            break;
        }

        local
            .write_all(&buf[..n])
            .await
            .map_err(|e| format!("Failed to write to local file {output_path_str}: {e}"))?;
        transferred += n as i64;

        let job_id_signal = job_id.clone();
        let file_name_owned = file_name.to_string();
        let transferred_now = transferred;
        qt_thread
            .queue(move |mgr| {
                mgr.file_transfer_progress(
                    &job_id_signal,
                    &qobject::QString::from(file_name_owned),
                    transferred_now,
                    file_size,
                );
            })
            .ok();
    }

    /* preserve original modification time on exported file */
    if transferred > 0 {
        use filetime::FileTime;

        let modified_utc = modified.and_utc();
        let mtime = FileTime::from_unix_time(
            modified_utc.timestamp(),
            modified_utc.timestamp_subsec_nanos(),
        );

        // ignore errors
        let _ = filetime::set_file_times(&output_path, mtime, mtime);
    }

    Ok(ExportItemResult {
        success: !cancel_flag.load(Ordering::Relaxed),
        bytes_transferred: transferred,
        destination_path: output_path_str,
        error_message: None,
    })
}

async fn handle_start_import(
    afc: &mut AfcClient,
    job_id: &qobject::QUuid,
    local_paths: &Vec<String>,
    destination_dir_on_device: String,
    qt_thread: &cxx_qt::CxxQtThread<qobject::IOManager>,
    jobs_map_for_task: &Arc<Mutex<HashMap<String, Arc<AtomicBool>>>>,
    cancel_flag: &Arc<AtomicBool>,
) {
    let mut items = Vec::with_capacity(local_paths.len() as usize);
    for i in 0..local_paths.len() {
        if let Some(p) = local_paths.get(i) {
            items.push(p.to_string());
        }
    }

    let job_id_str = job_id.to_string();
    let job_id_for_task = job_id.clone();

    let mut successful = 0_i32;
    let mut failed = 0_i32;
    let mut total_bytes = 0_i64;
    let mut cancelled = false;

    for local_path in items {
        if cancel_flag.load(Ordering::Relaxed) {
            cancelled = true;
            break;
        }

        let res = import_single_item(
            afc,
            &local_path,
            &destination_dir_on_device,
            &job_id_for_task,
            qt_thread,
            cancel_flag,
        )
        .await;

        match res {
            Ok(r) if r.success => {
                successful += 1;
                total_bytes += r.bytes_transferred;
                let file_name = Path::new(&local_path)
                    .file_name()
                    .and_then(|s| s.to_str())
                    .unwrap_or(&local_path)
                    .to_string();
                let job_id_signal = job_id_for_task.clone();
                let dest_clone = r.destination_path.clone();
                qt_thread
                    .queue(move |mgr| {
                        mgr.import_item_finished(
                            &job_id_signal,
                            &qobject::QString::from(file_name),
                            &qobject::QString::from(dest_clone),
                            true,
                            r.bytes_transferred,
                            &qobject::QString::from(""),
                        );
                    })
                    .ok();
            }
            Ok(r) => {
                failed += 1;
                let file_name = Path::new(&local_path)
                    .file_name()
                    .and_then(|s| s.to_str())
                    .unwrap_or(&local_path)
                    .to_string();
                let err_msg = r
                    .error_message
                    .unwrap_or_else(|| "Unknown error".to_string());
                let job_id_signal = job_id_for_task.clone();
                let dest_clone = r.destination_path.clone();
                qt_thread
                    .queue(move |mgr| {
                        mgr.import_item_finished(
                            &job_id_signal,
                            &qobject::QString::from(file_name),
                            &qobject::QString::from(dest_clone),
                            false,
                            r.bytes_transferred,
                            &qobject::QString::from(err_msg),
                        );
                    })
                    .ok();
            }
            Err(err) => {
                failed += 1;
                let file_name = Path::new(&local_path)
                    .file_name()
                    .and_then(|s| s.to_str())
                    .unwrap_or(&local_path)
                    .to_string();
                let job_id_signal = job_id_for_task.clone();
                qt_thread
                    .queue(move |mgr| {
                        mgr.import_item_finished(
                            &job_id_signal,
                            &qobject::QString::from(file_name),
                            &qobject::QString::from(""),
                            false,
                            0,
                            &qobject::QString::from(err),
                        );
                    })
                    .ok();
            }
        }
    }

    let job_id_signal = job_id_for_task.clone();
    qt_thread
        .queue(move |mgr| {
            mgr.import_job_finished(&job_id_signal, cancelled, successful, failed, total_bytes);
        })
        .ok();

    let mut guard = jobs_map_for_task
        .lock()
        .expect("IOManager jobs map mutex poisoned");
    guard.remove(&job_id_str);
}

async fn import_single_item(
    afc_client: &mut AfcClient,
    local_path: &str,
    destination_dir_on_device: &str,
    job_id: &QUuid,
    qt_thread: &cxx_qt::CxxQtThread<qobject::IOManager>,
    cancel_flag: &Arc<AtomicBool>,
) -> Result<ExportItemResult, String> {
    use tokio::io::AsyncReadExt;

    let file_name = Path::new(local_path)
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or(local_path);

    let device_path = if destination_dir_on_device.ends_with('/') {
        format!("{destination_dir_on_device}{file_name}")
    } else {
        format!("{destination_dir_on_device}/{file_name}")
    };

    let mut local = fs::File::open(local_path)
        .await
        .map_err(|e| format!("Failed to open local file {local_path}: {e}"))?;

    let metadata = local
        .metadata()
        .await
        .map_err(|e| format!("Failed to stat local file {local_path}: {e}"))?;
    let file_size = metadata.len() as i64;

    let mut remote = afc_client
        .open(&device_path, AfcFopenMode::WrOnly)
        .await
        .map_err(|e| format!("Failed to open device file {device_path} for writing: {e}"))?;

    let mut buf = [0u8; 8192];
    let mut transferred: i64 = 0;

    loop {
        if cancel_flag.load(Ordering::Relaxed) {
            break;
        }

        let n = local
            .read(&mut buf)
            .await
            .map_err(|e| format!("Failed to read from local file {local_path}: {e}"))?;
        if n == 0 {
            break;
        }

        remote
            .write_all(&buf[..n])
            .await
            .map_err(|e| format!("Failed to write to device file {device_path}: {e}"))?;
        transferred += n as i64;

        let job_id_signal = job_id.clone();
        let file_name_owned = file_name.to_string();
        let transferred_now = transferred;
        qt_thread
            .queue(move |mgr| {
                mgr.file_transfer_progress(
                    &job_id_signal,
                    &qobject::QString::from(file_name_owned),
                    transferred_now,
                    file_size,
                );
            })
            .ok();
    }

    Ok(ExportItemResult {
        success: !cancel_flag.load(Ordering::Relaxed),
        bytes_transferred: transferred,
        destination_path: device_path,
        error_message: None,
    })
}

async fn generate_unique_output_path(base: &Path) -> PathBuf {
    if fs::metadata(base).await.is_err() {
        return base.to_path_buf();
    }

    let parent = base.parent().unwrap_or_else(|| Path::new("."));
    let stem = base.file_stem().and_then(|s| s.to_str()).unwrap_or("file");
    let ext = base.extension().and_then(|s| s.to_str()).unwrap_or("");

    for counter in 1..10_000 {
        let candidate_name = if ext.is_empty() {
            format!("{stem}_{counter}")
        } else {
            format!("{stem}_{counter}.{ext}")
        };
        let candidate = parent.join(candidate_name);
        if fs::metadata(&candidate).await.is_err() {
            return candidate;
        }
    }

    base.to_path_buf()
}
