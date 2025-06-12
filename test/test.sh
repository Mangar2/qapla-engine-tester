#!/bin/bash

tests=(
  "--help:0"
  "--invalid:2"
  "--logpath=log:0"
  "--logpath=log --enginelog --test numgames=0 nostop nooption --engine conf=Qapla 0.3.2 --enginesfile=./test/engines.ini:0"
  #"--logpath=log --enginelog --test numgames=0 nostop --engine conf=\"Qapla 0.3.1\" --enginesfile=./test/engines.ini:10"
  #"--logpath=log --enginelog --test numgames=20 nostop --engine conf=\"Qapla 0.3.2\" --enginesfile=./test/engines.ini:0"
  #"--logpath=log --test numgames=20 --engine name=Qapla0.3.2 cmd=./Qapla0.3.2-linux-x86:10"
  #"--logpath=log --enginelog --concurrency=20 --epd file=wmtest.epd maxtime=20 mintime=1 seenplies=3 minsuccess=80 --engine conf=\"Qapla 0.3.2\" --enginesfile=./test/engines.ini:13"
  #"--logpath=log --concurrency=20 --tc=10+0.02 --sprt elolower=0 eloupper=10 alpha=0.05 beta=0.05 maxgames=300 --engine conf=\"Qapla 0.3.1\" --engine conf=\"Qapla 0.3.2\" --openings file=\"book8ply.raw\" format=raw --enginesfile=./test/engines.ini:16"
  #"--logpath=log --concurrency=20 --tc=10+0.02 --pgnoutput file=log/test.pgn append=false pv --sprt elolower=0 eloupper=40 alpha=0.05 beta=0.05 maxgames=3000 --engine conf=\"Qapla 0.3.2\" --engine conf=\"Spike 1.4\"  --openings file=\"book8ply.raw\" order=random format=raw --enginesfile=./test/engines.ini:15"
  #"--logpath=log --concurrency=20 --tc=10+0.02 --pgnoutput file=log/test.pgn append pv --sprt elolower=0 eloupper=30 alpha=0.05 beta=0.02 maxgames=3000 --engine conf=\"Spike 1.4\" --engine conf=\"Qapla 0.3.2\" --openings file=\"book8ply.raw\" format=raw --enginesfile=./test/engines.ini:14"
)

results=()

for entry in "${tests[@]}"; do
  IFS=":" read -r args expected <<< "$entry"
  ./qapla-engine-tester $args
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
