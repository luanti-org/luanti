{ pkgs ? import <nixpkgs> {}, }:

pkgs.mkShell {
  LOCALE_ARCHIVE = "${pkgs.glibcLocales}/lib/locale/locale-archive";
  env.LANG = "C.UTF-8";
  env.LC_ALL = "C.UTF-8";

  packages = [
    pkgs.gcc
    pkgs.cmake
    pkgs.zlib
    pkgs.zstd
    pkgs.libjpeg
    pkgs.libpng
    pkgs.libGL
    pkgs.luajit
    pkgs.SDL2
    pkgs.openal
    pkgs.curl
    pkgs.libvorbis
    pkgs.libogg
    pkgs.gettext
    pkgs.gmp
    pkgs.freetype
    pkgs.sqlite
  ];
}
