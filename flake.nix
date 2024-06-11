{
  description = "Igraph SE2 implementation";

  inputs = { nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05"; };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in {
      devShells.${system}.default = pkgs.mkShell {
        packages = (with pkgs; [
          astyle
          cmake
          ninja
          gdb
          valgrind
          # igraph dependencies
          bison
          flex
          libxml2
        ]);
        shellHook = ''
          export OMP_NUM_THREADS=16
        '';
      };
    };
}
