#! /bin/bash -eu

cmake -B build -DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-DRUN_IN_PLACE=TRUE \
	-DENABLE_GETTEXT=FALSE \
	-DBUILD_SERVER=TRUE
cmake --build build --target GenerateVersion

./util/ci/run-clang-tidy.py \
	-clang-tidy-binary=$CLANG_TIDY -p build \
	-quiet -config="$(cat .clang-tidy)" \
	'src/.*'
