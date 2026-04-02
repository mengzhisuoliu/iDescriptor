use cxx_qt_build::CxxQtBuilder;

fn main() {
    CxxQtBuilder::new()
        .file("src/lib.rs")
        .file("src/afc_services.rs")
        .file("src/service_manager.rs")
        .file("src/screenshot.rs")
        .file("src/hause_arrest.rs")
        .file("src/io_manager.rs")
        .build();
}
