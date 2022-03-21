#!/bin/bash
# Bash app to manage scene with fweelin
# inspired by scripts : https://github.com/free-wheeling/freewheeling/blob/master/scripts/

# je me place dans fw-lib
cd $HOME/fw-lib/

CHOICE=$(zenity --list --width=600 --height=400 --hide-column=1  \
  --title="fweelin scene manager" \
  --column="ref" --column="Choose an action" \
    1 "Archive scene with loops" \
    2 "Delete scene with loops")

if [ "$?" -eq 1 ]; then
    #On quitte le script
    exit
fi

if [ "$CHOICE" -eq 1 ]; then
TITLEN="Choose a scene to archive"
else
TITLEN="Choose a scene to delete"
fi

FILEI=$(zenity --title="$TITLEN" --filename="$HOME/fw-lib/" --file-filter='scene | scene*.xml' --file-selection)

if [ "$?" -eq 1 ]; then
    #On quitte le script
    exit
fi

# extraction du nom 
pos=`echo $FILEI |awk 'BEGIN{FS="/"} {print NF}'`
FILE=`echo "$FILEI" | cut -d'/' -f $pos-`

pos1=$(expr ${#FILE} - 4)
NOM=$(echo "$FILE" | cut -c 40-$pos1) # les 40 premiers caracteres sont scene - cle. et a la fin on enleve le .xml soit 4 caracteres

# selection des fichiers relatifs a la scene
pos=`echo $FILEI |awk 'BEGIN{FS="/"} {print NF}'`
FILE=`echo "$FILEI" | cut -d'/' -f $pos-`
awk '{ if ($1 == "<loop") { sub(/hash="/,"",$3) ; sub(/"/,"",$3) ; print "loop-" $3 "*" } }' "$FILE"
SCENEFILE=`echo "$FILE" | cut -d- -f1,2`

echo $FILE 

case $CHOICE in
    1)
    # selection de l'archive
    ARC=$(zenity --title="Choisir une archive" --filename="$HOME/fw-lib/$NOM.tar.bz2" --file-filter='tar.bz2 | *.tar.bz2' --save  --confirm-overwrite --file-selection)
    if [ "$?" -eq 1 ]; then
    #On quitte le script
    exit
    fi
    tar cjvf $ARC `awk '{ if ($1 == "<loop") { sub(/hash="/,"",$3) ; sub(/"/,"",$3) ; print "loop-" $3 "*" } }' "$FILE"`   $SCENEFILE*
    ;;
    2) rm `awk '{ if ($1 == "<loop") { sub(/hash="/,"",$3) ; sub(/"/,"",$3) ; print "loop-" $3 "*" } }' "$FILE"`
rm $SCENEFILE*
    ;;
    *) exit
    ;;
esac

exit

# https://github.com/arnaudfi/fweelin_config 

