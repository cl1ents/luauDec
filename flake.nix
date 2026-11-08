{
  description = "luauDec: Luau bytecode decompiler (static library + CLI)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    # keep in sync with the `luau` submodule
    luau = {
      url = "github:luau-lang/luau/0.745";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      luau,
    }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      # component: CMake install component to install, or null for everything
      mkLuauDec =
        pkgs:
        {
          pname,
          component ? null,
          buildCli ? true,
        }:
        pkgs.stdenv.mkDerivation {
          inherit pname;
          version = "1.0.0";
          src = self;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
          ];

          cmakeFlags = [
            "-DLUAUDEC_LUAU_SOURCE_DIR=${luau}"
            "-DLUAUDEC_BUILD_CLI=${if buildCli then "ON" else "OFF"}"
          ];

          doCheck = true;
          checkPhase = ''
            runHook preCheck
            ctest --output-on-failure
            runHook postCheck
          '';

          installPhase = ''
            runHook preInstall
            cmake --install . ${pkgs.lib.optionalString (component != null) "--component ${component}"}
            runHook postInstall
          '';

          meta = {
            description = "Luau bytecode decompiler";
            homepage = "https://github.com/cl1ents/luauDec";
            mainProgram = "luauDec";
            platforms = systems;
          };
        };
    in
    {
      packages = forAllSystems (
        pkgs:
        let
          build = mkLuauDec pkgs;
        in
        rec {
          # libluaudec.a, include/luauDec/Decompile.h, CMake package (find_package(luauDec)) and pkg-config (luaudec)
          lib = build {
            pname = "luaudec-lib";
            component = "library";
            buildCli = false;
          };
          # bin/luauDec
          cli = build {
            pname = "luauDec";
            component = "cli";
          };
          luauDec = build { pname = "luauDec-full"; };
          default = luauDec;
        }
      );

      apps = forAllSystems (pkgs: {
        default = {
          type = "app";
          program = "${self.packages.${pkgs.stdenv.hostPlatform.system}.cli}/bin/luauDec";
        };
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];
        };
      });
    };
}
