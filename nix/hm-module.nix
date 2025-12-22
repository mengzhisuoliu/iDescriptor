{ config, lib, ... }:

with lib;

let
  cfg = config.programs.idescriptor;
in
{
  options.programs.idescriptor = {
    enable = mkEnableOption "iDescriptor program";

    package = mkOption {
      type = types.package;
      defaultText = literalExpression "pkgs.idescriptor";
      description = "The iDescriptor package to use";
    };
  };

  config = mkIf cfg.enable {
    home.packages = [ cfg.package ];
  };
}
