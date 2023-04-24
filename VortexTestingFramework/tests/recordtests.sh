#!/bin/bash

VORTEX=../vortex

if [ ! -x $VORTEX ]; then
  make -C ../
fi

if [ ! -x $VORTEX ]; then
  echo "Could not find $VORTEX"
  exit
fi

rm -rf tmp
mkdir tmp

for FILE in *.test; do
  INPUT="$(grep "Input=" $FILE | cut -d= -f2)"
  BRIEF="$(grep "Brief=" $FILE | cut -d= -f2)"
  echo -e "\e[31mRecording \e[33m[\e[97m$BRIEF\e[33m]...\e[0m"
  TEMP_FILE="tmp/${FILE}.out"
  # Truncate everything after and including the line with "Initializing..."
  awk '/Initializing.../{exit} 1' $FILE > $TEMP_FILE
  # Append the output of the $VORTEX command to the temp file
  # NOTE: When recording the tests we don't use valgrind because
  #       the valgrind output should be clean anyway. But when running
  #       the test valgrind is used with --leak-check=full --show-leak-kinds=all
  $VORTEX <<< $INPUT >> $TEMP_FILE
  # Replace the original file with the modified temp file
  mv $TEMP_FILE $FILE
done

rm -rf tmp
