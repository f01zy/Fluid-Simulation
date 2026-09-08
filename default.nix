{
  pkgs ? import <nixpkgs> { },
}:
let
  dependencies = with pkgs; [
    wayland
    wayland-protocols
    libxkbcommon
    libdecor

    libxcb
    libXtst
    libX11
    libXext
    libXcursor
    libXrandr
    libXi
    libXfixes
    libXScrnSaver
    libxinerama

    libffi
    libglvnd
    libGL
  ];
in
pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    cmake
    pkg-config
    wayland-scanner
  ];
  buildInputs = dependencies;
  shellHook = ''
    export CMAKE_PREFIX_PATH="${pkgs.libdecor}:$CMAKE_PREFIX_PATH"
    export LD_LIBRARY_PATH="${pkgs.lib.makeLibraryPath dependencies}:/run/opengl-driver/lib:/run/opengl-driver-32/lib:$LD_LIBRARY_PATH"
  '';
}
