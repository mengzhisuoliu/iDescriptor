# default.nix - for backwards compatibility with nix-build
(import (
  fetchTarball {
    url = "https://github.com/edolstra/flake-compat/archive/master.tar.gz";
    sha256 = "0bi4cpqmwpqkv2ikml4ryh14j5l9bl1f16wfixa97h6yvk7ik9aw";
  }
) {
  src = ./.;
}).defaultNix
