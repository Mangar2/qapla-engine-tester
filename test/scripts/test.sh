#!/bin/bash

tests=(
  "--help:0"
  "--invalid:2"
  "--logpath=log:0"
  "--settingsfile=test-noerror.ini:0"
  "--settingsfile=test-error-10.ini:10"
  "--settingsfile=test-error-10-stop.ini:10"
  "--settingsfile=test-error-ponder.ini:12"
  "--settingsfile=test-20-games.ini:0"
  "--settingsfile=test-epd-13.ini:13"
  "--settingsfile=test-gauntlet.ini:0"
  "--settingsfile=test-tournament-file.ini:0"
# Second test playing no game due to tournament file
  "--settingsfile=test-tournament-file.ini:0"
  "--settingsfile=test-sprt-maxgames.ini:16"
  "--settingsfile=test-sprt-rapid.ini:16"
)

results=()

rm -f log/*

for entry in "${tests[@]}"; do
  IFS=":" read -r args expected <<< "$entry"
  ../build/release/qapla-engine-tester $args --enginesfile=linux-engines.ini --logpath=log
  code=$?
  if [ "$code" -eq "$expected" ]; then
    results+=("✔ Test mit '$args' erfolgreich (Code $code)")
  else
    results+=("✘ FEHLER bei '$args' (Code $code, erwartet $expected)")
  fi
done

echo -e "\n--- Testergebnisse ---"
for line in "${results[@]}"; do
  echo "$line"
done

