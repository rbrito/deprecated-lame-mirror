#!/bin/sh

# set -x

my_name=$(basename $0)

# set encoder
if [ "X${LAME}" != "X" ]; then
	enc="${LAME}"
else
	enc=lame
fi

enc_cbr_opts="-b192 -h --lowpass 18 --lowpass-width 0"
enc_vbr_opts="--vbr-mtrh --nspsytune -v -h -d -k -V3 -q2 -Y -X3"

if [ "X${LAMEOPT}" != "X" ]; then
	enc_opts="${LAMEOPT}"
else
	enc_opts="${enc_cbr_opts}"
fi

# usage
print_usage() {
	echo "MUltiGEnerationCOding"
	echo "(c) 2000 by Alexander Leidinger"
	echo
	echo "Usage: ${my_name} [-v] -g <num> <file>"
	echo " -v: use buildin VBR options"
	echo " -g <num>: encode <num> generations"
	echo
	echo " used"
	echo "  - env vars:"
	echo "    * LAME   : alternative encoder binary"
	echo "    * LAMEOPT: alternative encoder options"
	echo "  - VBR opts: ${enc_vbr_opts}"
	echo "  - CBR opts: ${enc_cbr_opts}"
}

# processing options
args=$(getopt vg: $*)
if [ $? != 0 ]; then
	print_usage
	exit 1
fi

set -- ${args}

for i; do
	case "${i}" in
		-v)
			enc_opts="${enc_vbr_opts}"
			shift
			;;
		-g)
			num="$2"
			shift
			shift
			;;
		--)
			shift
			break
			;;
	esac
done

# check requirements
if [ "X${num}" = "X" ]; then
	print_usage
	exit 1
fi

### check if num is a number



# do we have a file
if [ ! -f "$1" ]; then
	print_usage
	exit 1
fi

## OK, do something usefull

# basename
base=$(basename "$1" .wav)
SECURE=$(mktemp -u ${base}_XXXXXXXXXX) || exit 1

# what version of lame are we using?
lame_vers=$(${enc} 2>&1 | head -1 | cut -f 3 -d ' ')

iterate=0

cp ${base}.wav ${SECURE}.wav

while [ "${iterate}" != "${num}" ]; do
	gen="$((${iterate}+1))"

	echo "Working on generation number ${gen}..."

	${enc} ${enc_opts} --tc "lame ${lame_vers}; Generation: ${gen}" "${SECURE}.wav" "${SECURE}.mp3" || exit 1
	${enc} --decode "${SECURE}.mp3" "${SECURE}.wav" || exit 1

	iterate=$(echo ${iterate} | awk '{i=$1 + 1; printf "%d", i}')
done

mv "${SECURE}.wav" "${base}_generation_${num}.wav"

# cleanup
rm "${SECURE}.mp3"

exit 0

