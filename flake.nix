{
  description = "Igraph SE2 implementation";

  inputs = { nixpkgs.url = "github:NixOS/nixpkgs"; };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in {
      devShells.${system}.default =
        (pkgs.mkShell.override { stdenv = pkgs.clangStdenv; }) {
          packages = (with pkgs; [ llvm cmake ninja gdb valgrind ]);
        };
    };
}
