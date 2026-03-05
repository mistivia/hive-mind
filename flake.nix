{
  description = "HiveMind";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        packages.hivemind = pkgs.stdenv.mkDerivation {
          pname = "hivemind";
          version = "0.1.0";
          src = self;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.gnumake
            pkgs.makeWrapper
          ];

          buildInputs = [
            pkgs.boost
          ];

          dontConfigure = true;

          makeFlags = [
            "LIBS=-lm -lpthread"
          ];

          buildPhase = ''
            runHook preBuild

            make LIBS="-lm -lpthread" hivemind
            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall

            install -Dm755 hivemind $out/libexec/hivemind
            mkdir -p $out/share/hivemind
            cp -r resource/. $out/share/hivemind/

            makeWrapper $out/libexec/hivemind $out/bin/hivemind \
              --set HIVEMIND_RESOURCE "$out/share/hivemind"

            runHook postInstall
          '';
        };

        packages.default = self.packages.${system}.hivemind;

        apps.default = {
          type = "app";
          program = "${self.packages.${system}.hivemind}/bin/hivemind";
        };

        devShells.default = pkgs.mkShell {
          packages = [
            pkgs.cmake
            pkgs.gnumake
            pkgs.boost
          ];
        };
      });
}
