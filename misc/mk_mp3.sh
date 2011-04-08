### (c)2000-2011 Robert Hegemann
###
### /<path>/<artist>/<year>-<album>/<track-no> <title>
###
### SRC_ROOT : where the CD are located
### DST_ROOT : where newly encoded tracks should go
### DONE_ROOT : where verified encoded tracks are located
### LAME_EXE : points to program
### LAME_OPT : what options to use
################################################################

SRC_ROOT=/windows/W/CD
DST_ROOT=/windows/Z/lame399r0.mp3v0.wg
DONE_ROOT=/windows/Q/lame399r0.mp3v0.wg
LAME_EXE=lame-399r0
LAME_OPT="-V0 --quiet --noreplaygain"

if (test -e "${SRC_ROOT}")
then
    if test -e "${DST_ROOT}"
    then
        :
    else
        mkdir "${DST_ROOT}"
    fi

    for _artist in "${SRC_ROOT}"/*
    do
        _ARTIST=$(basename "${_artist}")
        DST_ARTIST=${DST_ROOT}/${_ARTIST}
        DS2_ARTIST=${DONE_ROOT}/${_ARTIST}
        ID3_ARTIST=`echo "${_ARTIST}"`
        echo "Artist: ${_ARTIST}"

        for _cd in "${_artist}"/*
        do
            _CD=$(basename "${_cd}")
            DST_CD=${DST_ARTIST}/${_CD}
            DS2_CD=${DS2_ARTIST}/${_CD}
            ID3_YR=`echo "${_CD}"|cut -b 1-4`
            ID3_CD=`echo "${_CD}"|cut -b 6-`
            if test -e "${DS2_CD}"
            then
                continue
            fi
            if test -e "${DST_CD}"
            then
                continue
            fi
            echo "CD: ${_CD}"

            ALBUM_GAIN="1.0"
            ALBUM_GAIN=`wavegain -x -a "${_cd}"/ 2>/dev/null`

            for _song in "${_cd}"/*
            do
                _SONG=$(basename "${_song}" .wav)
                DST_SONG=${DST_CD}/$(basename "${_song}" .wav).mp3
                if test -e "${DST_SONG}"
                then
                    :
                else
                    if test -e "${DST_ARTIST}"
                    then
                        :
                    else
                        mkdir "${DST_ARTIST}"
                    fi
                    if test -e "${DST_CD}"
                    then
                        :
                    else
                        mkdir "${DST_CD}"
                    fi
                    ID3_TRACK=`echo "${_SONG}"|cut -b 1-2`
                    if [ "${_ARTIST}" = "Various" ]
                    then
                        ID3_TITLE=`echo "${_SONG% - *}"|cut -b 4-`
                        ID3_TRACK_ARTIST=`echo "${_SONG#* - }"`
                        ID3_ALBUM_ARTIST="Various"
                    else
                        ID3_TITLE=`echo "${_SONG}"|cut -b 4-`
                        ID3_TRACK_ARTIST=${ID3_ARTIST}
                        ID3_ALBUM_ARTIST=${ID3_ARTIST}
                    fi
#echo "TITLE: ${ID3_TITLE}"
#echo "ARTIST: ${ID3_TRACK_ARTIST}"
                    ${LAME_EXE} ${LAME_OPT} \
                            --scale ${ALBUM_GAIN} \
                            --id3v2-ucs2 --pad-id3v2 \
                            --ta "${ID3_TRACK_ARTIST}" \
                            --tl "${ID3_CD}" \
                            --ty "${ID3_YR}" \
                            --tt "${ID3_TITLE}" \
                            --tn "${ID3_TRACK}" \
                            --tv "TXXX=ALBUM ARTIST=${ID3_ALBUM_ARTIST}" \
                            --tv "TXXX=LAME SCALE=${ALBUM_GAIN}" \
                            "${_song}" "${DST_SONG}" &
                fi
            done
            wait
        done
    done
else
    echo Quellverzeichnis ${SRC_ROOT} existiert nicht.
fi
