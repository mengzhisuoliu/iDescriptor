{
pkgs,
...
}:

pkgs.stdenv.mkDerivation rec {
  pname = "idescriptor";
  version = "0.1.2";

  src = pkgs.fetchFromGitHub {
    owner = "iDescriptor";
    repo = "iDescriptor";
    rev = "v${version}";
    hash = "sha256-pj/8PCZUTPu28MQd3zL8ceDsQy4+55348ZOCpiQaiEo=";
    fetchSubmodules = true;
  };

  goModules = (pkgs.stdenv.mkDerivation {
    name = "${pname}-${version}-go-modules";
    inherit src;

    nativeBuildInputs = with pkgs; [ go ];

    configurePhase = ''
      export GOPATH="$TMPDIR/go"
      export GOCACHE="$TMPDIR/go-cache"
      export HOME="$TMPDIR"
      cd lib/ipatool-go
      '';

    buildPhase = ''
      go mod download
      '';

    installPhase = ''
      mkdir -p $out
      cp -r --reflink=auto $GOPATH/pkg/mod $out/
      '';

    outputHashMode = "recursive";
    outputHashAlgo = "sha256";
    outputHash = "sha256-CmthXlyxbWRqAY4gFLshx7VGAdbQlySm4y6mWFdxdUI=";
  });

  nativeBuildInputs = with pkgs; [
    cmake
    pkg-config
    go
    qt6.wrapQtAppsHook
    copyDesktopItems
  ];

  buildInputs = with pkgs; [
    qt6.qtbase
    qt6.qtdeclarative
    qt6.qtmultimedia
    qt6.qtserialport
    qt6.qtpositioning
    qt6.qtlocation
    lxqt.qtermwidget
    qrencode
    libheif
    libde265
    x265
    libirecovery
    libssh
    ffmpeg
    pugixml
    avahi
    avahi-compat
    libimobiledevice
    libirecovery
    libplist
    usbmuxd
    libzip
    openssl
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
  ];

  preBuild = ''
    export HOME=$TMPDIR
    export GOCACHE=$TMPDIR/go-cache
    export GOPATH=$TMPDIR/go

    # Copy Go modules to writable location
    mkdir -p $GOPATH/pkg
    cp -r --reflink=auto ${goModules}/mod $GOPATH/pkg/
    chmod -R +w $GOPATH/pkg/mod
    
    export GOMODCACHE=$GOPATH/pkg/mod
    export GOPROXY=off
  '';

  desktopItems = [
    (pkgs.makeDesktopItem {
      name = "iDescriptor";
      exec = "iDescriptor";
      icon = "idescriptor";
      desktopName = "iDescriptor";
      comment = "Cross-platform iDevice management tool";
      categories = [ "System" "Utility" ];
    })
  ];

  postInstall = ''
    # Install icon
    install -Dm644 $src/resources/icons/app-icon/icon.png \
      $out/share/icons/hicolor/256x256/apps/idescriptor.png
  '';

  meta = with pkgs.lib; {
    description = "Free, open-source, cross-platform iDevice management tool";
    homepage = "https://github.com/iDescriptor/iDescriptor";
    license = licenses.agpl3Only;
    maintainers = [ fn3x ];
    platforms = platforms.unix;
    mainProgram = "iDescriptor";
  };
}
