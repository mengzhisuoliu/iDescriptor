use crate::{APP_DEVICE_STATE, RUNTIME};
use cxx_qt::CxxQtType;
use cxx_qt::Threading;
use cxx_qt_lib::{QByteArray, QString};
use idevice::services::core_device_proxy::CoreDeviceProxy;
use idevice::{
    IdeviceService, RsdService, dvt::remote_server::RemoteServerClient, provider::IdeviceProvider,
    rsd::RsdHandshake,
};
use idevice::{dvt::screenshot::ScreenshotClient, screenshotr::ScreenshotService};
use std::pin::Pin;

#[cxx_qt::bridge(namespace = "CXX")]
mod qobject {
    #[namespace = ""]
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        include!("cxx-qt-lib/qbytearray.h");

        type QString = cxx_qt_lib::QString;
        type QByteArray = cxx_qt_lib::QByteArray;
    }

    extern "RustQt" {
        #[qobject]
        type ScreenshotBackend = super::RScreenshotBackend;

        #[qinvokable]
        fn set_udid(self: Pin<&mut ScreenshotBackend>, udid: &QString);

        #[qinvokable]
        fn start_capture(self: Pin<&mut ScreenshotBackend>);

        #[qsignal]
        fn screenshot_captured(self: Pin<&mut ScreenshotBackend>, data: QByteArray);

        #[qsignal]
        fn init_failed(self: Pin<&mut ScreenshotBackend>, reason: QString);
    }

    impl cxx_qt::Threading for ScreenshotBackend {}
    impl cxx_qt::Constructor<(QString, u32), NewArguments = (QString, u32)> for ScreenshotBackend {}
}

#[derive(Default)]
pub struct RScreenshotBackend {
    udid: QString,
    ios_version: u32,
}
impl cxx_qt::Constructor<(QString, u32)> for qobject::ScreenshotBackend {
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

    fn new(args: (QString, u32)) -> RScreenshotBackend {
        RScreenshotBackend {
            udid: args.0,
            ios_version: args.1,
        }
    }
}

impl qobject::ScreenshotBackend {
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

    fn start_capture(self: Pin<&mut Self>) {
        let qt_t = self.qt_thread();
        let udid_q = self.rust().udid.clone();
        let udid_str = udid_q.to_string();
        let ios_version = self.rust().ios_version;

        println!(
            "Starting screenshot capture for device {}, iOS version {}",
            udid_str, ios_version
        );

        RUNTIME.spawn(async move {
            let qt_thread = qt_t.clone();

            let device = {
                let maybe_device = APP_DEVICE_STATE
                    .lock()
                    .await
                    .get(udid_str.as_str())
                    .cloned();

                match maybe_device {
                    Some(d) => d,
                    None => {
                        eprintln!("screenshot: device {} not found", udid_str);
                        qt_thread
                            .queue(move |backend_qobj| {
                                backend_qobj.init_failed(QString::from("Device not found"));
                            })
                            .ok();
                        return;
                    }
                }
            };

            let provider_guard = device.provider.lock().await;

            if ios_version > 16 {
                run_capture_ios17_and_above(qt_thread, provider_guard.as_ref()).await;
            } else {
                run_capture_ios16_and_lower(qt_thread, provider_guard.as_ref()).await;
            }
        });
    }
}

async fn run_capture_ios17_and_above(
    qt_thread: cxx_qt::CxxQtThread<qobject::ScreenshotBackend>,
    provider: &dyn IdeviceProvider,
) {
    let proxy = match CoreDeviceProxy::connect(provider).await {
        Ok(p) => p,
        Err(e) => {
            eprintln!("screenshot CoreDeviceProxy connect failed: {e}");
            qt_thread
                .queue(move |b| {
                    b.init_failed(QString::from(format!(
                        "Failed to connect to CoreDeviceProxy: {e}"
                    )))
                })
                .ok();
            return;
        }
    };

    let rsd_port = proxy.handshake.server_rsd_port;
    let mut adapter = match proxy.create_software_tunnel() {
        Ok(a) => a.to_async_handle(),
        Err(e) => {
            eprintln!("screenshot dvt tunnel err: {e}");
            qt_thread
                .queue(move |b| {
                    b.init_failed(QString::from(format!(
                        "Failed to create software tunnel: {e}"
                    )))
                })
                .ok();
            return;
        }
    };

    let stream = match adapter.connect(rsd_port).await {
        Ok(s) => s,
        Err(e) => {
            eprintln!("screenshot dvt connect err: {e}");
            qt_thread
                .queue(move |b| {
                    b.init_failed(QString::from(format!("Failed to connect to RSD port: {e}")))
                })
                .ok();
            return;
        }
    };

    let mut handshake = match RsdHandshake::new(stream).await {
        Ok(h) => h,
        Err(e) => {
            eprintln!("screenshot handshake err: {e}");
            qt_thread
                .queue(move |b| {
                    b.init_failed(QString::from(format!(
                        "Failed to complete RSD handshake: {e}"
                    )))
                })
                .ok();
            return;
        }
    };

    let mut remote_server =
        match RemoteServerClient::connect_rsd(&mut adapter, &mut handshake).await {
            Ok(s) => s,
            Err(e) => {
                eprintln!("screenshot remote err: {e}");
                qt_thread
                    .queue(move |b| {
                        b.init_failed(QString::from(format!(
                            "Failed to connect to Remote Server: {e}"
                        )))
                    })
                    .ok();
                return;
            }
        };

    if let Err(e) = remote_server.read_message(0).await {
        eprintln!("screenshot read_message err: {e}");
        qt_thread
            .queue(move |b| {
                b.init_failed(QString::from(format!(
                    "Failed to read initial message: {e}"
                )))
            })
            .ok();
        return;
    }

    match ScreenshotClient::new(&mut remote_server).await {
        Ok(mut client) => {
            while !qt_thread.is_destroyed() {
                match client.take_screenshot().await {
                    Ok(b) => {
                        qt_thread
                            .queue(move |backend| {
                                backend.screenshot_captured(QByteArray::from(&b[..]))
                            })
                            .ok();
                    }
                    Err(e) => {
                        eprintln!("screenshot take err: {e}");
                        qt_thread
                            .queue(move |b| {
                                b.init_failed(QString::from(format!(
                                    "Failed to take screenshot: {e}"
                                )))
                            })
                            .ok();
                        return;
                    }
                }
            }
        }
        Err(e) => {
            eprintln!("screenshot client err: {e}");
            qt_thread
                .queue(move |b| {
                    b.init_failed(QString::from(format!(
                        "Failed to initialize screenshot client: {e}"
                    )))
                })
                .ok();
        }
    }
}

async fn run_capture_ios16_and_lower(
    qt_thread: cxx_qt::CxxQtThread<qobject::ScreenshotBackend>,
    provider: &dyn IdeviceProvider,
) {
    match ScreenshotService::connect(provider).await {
        Ok(mut service) => {
            while !qt_thread.is_destroyed() {
                match service.take_screenshot().await {
                    Ok(b) => {
                        qt_thread
                            .queue(move |backend| {
                                backend.screenshot_captured(QByteArray::from(&b[..]))
                            })
                            .ok();
                    }
                    Err(e) => {
                        eprintln!("screenshotr take err: {e}");
                        qt_thread
                            .queue(move |b| {
                                b.init_failed(QString::from(format!(
                                    "Failed to take screenshot: {e}"
                                )))
                            })
                            .ok();
                        return;
                    }
                }
            }
        }
        Err(e) => {
            eprintln!("screenshotr connect failed: {e}");
            qt_thread
                .queue(move |b| {
                    b.init_failed(QString::from(format!(
                        "Failed to connect to ScreenshotR service: {e}"
                    )))
                })
                .ok();
        }
    }
}
