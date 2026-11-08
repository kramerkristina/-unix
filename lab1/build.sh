#!/bin/sh
# Сборка исходника (C, C++, TeX) во временном каталоге
# Использование: build2.sh <файл>
# Коды выхода: 10 - аргументы, 11 - нет файла, 12 - нет Output:, 13 - тип файла,
#              14 - mktemp/cd, 15 - ошибка сборки, 16 - нет результата
# Сигналы: 129 HUP, 130 INT, 131 QUIT, 143 TERM

workdir=""

finish() {
    if [ -n "$workdir" ]; then
        rm -rf -- "$workdir"
    fi
}

fail() {
    echo "Ошибка: $2" >&2
    exit "$1"
}

trap finish EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 131' QUIT
trap 'exit 143' TERM

if [ "$#" -ne 1 ]; then
    fail 10 "нужен один аргумент - исходный файл"
fi
if [ ! -f "$1" ]; then
    fail 11 "нет такого файла: $1"
fi

srcdir=$(cd "$(dirname "$1")" && pwd)
srcfile=$(basename "$1")
fullpath="$srcdir/$srcfile"

target=$(sed -n 's/^.*\(\/\*\|\/\/\|%\|#\)[[:space:]]*Output:[[:space:]]*\([^[:space:]]*\).*$/\2/p' "$fullpath" | head -n 1)
target=${target%\*/}
if [ -z "$target" ]; then
    fail 12 "в файле нет комментария Output:"
fi
case "$target" in
    */*|.|..) fail 12 "плохое имя результата: $target" ;;
esac

workdir=$(mktemp -d) || fail 14 "mktemp не сработал"
cd "$workdir" || fail 14 "не удалось зайти в $workdir"

case "$srcfile" in
    *.c)
        cc -I"$srcdir" -o "$target" "$fullpath" || fail 15 "не собралось"
        result=$target
        ;;
    *.cpp|*.cc|*.cxx)
        c++ -I"$srcdir" -o "$target" "$fullpath" || fail 15 "не собралось"
        result=$target
        ;;
    *.tex)
        TEXINPUTS="$srcdir:" pdflatex -interaction=nonstopmode -halt-on-error \
            -jobname=out "$fullpath" >log.txt 2>&1 || { cat log.txt >&2; fail 15 "не собралось"; }
        result=out.pdf
        ;;
    *)
        fail 13 "неизвестный тип файла: $srcfile"
        ;;
esac

if [ ! -f "$result" ]; then
    fail 16 "результат не создан"
fi
cp -- "$result" "$srcdir/$target" || fail 16 "не удалось скопировать результат"
