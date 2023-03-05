with import <nixpkgs> {};
stdenv.mkDerivation {
	name = "spade";

	src = lib.cleanSource ./.;
	nativeBuildInputs = [cmake];
	buildInputs = [
		xorg.libX11
		libGL
		libxkbcommon
	];

	cmakeFlags = [ "--preset=pc" "-B ." ];

	buildPhase = "make";
	installPhase = "mkdir $out; mv spade $out/spade";
}