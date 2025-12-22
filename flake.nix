{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ];
    in
      {
      packages = forAllSystems (system: {
        default = nixpkgs.legacyPackages.${system}.callPackage ./nix/package.nix { };
      });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/idescriptor";
        };
      });

      devShells = forAllSystems (system: 
        let pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
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
          };
        });

      nixosModules.default = { config, lib, pkgs, ... }:
        let
          cfg = config.programs.idescriptor;
          idescriptorPkg = self.packages.${pkgs.system}.default;
        in
          {
          imports = [ ./nix/nixos-module.nix ];

          config = lib.mkIf cfg.enable {
            programs.idescriptor.package = lib.mkDefault idescriptorPkg;
          };
        };

      homeManagerModules.default = { config, lib, pkgs, ... }:
        let
          cfg = config.programs.idescriptor;
          idescriptorPkg = self.packages.${pkgs.system}.default;
        in
          {
          imports = [ ./nix/hm-module.nix ];

          config = lib.mkIf cfg.enable {
            programs.idescriptor.package = lib.mkDefault idescriptorPkg;
          };
        };
    };
}
