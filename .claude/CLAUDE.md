# Arbeitsweise in diesem Repository

## Probleme gehören ans Ende der Antwort

Alles, was während eines Tasks aufgefallen ist, steht **am Schluss der Antwort** — nicht
mittendrin, nicht in einem Tool-Output, nicht in einem Nebensatz. Dazu gehören insbesondere:

- fehlgeschlagene Tests, jeder einzelne mit Namen
- Tests, die gar nicht gelaufen sind, und warum
- Fehler, Warnungen oder Ungereimtheiten, die beim Lesen oder Bauen des Codes auffielen

Auch wenn ein Task erfolgreich war: solange ein Problem offen ist, endet die Antwort damit.
Was weiter oben steht, wird beim Scrollen übersehen.

## Aufgefallene Fehler kommen als Nächstes dran

Ein Fehler, der auffällt, wird als **nächste Aufgabe** behoben — nach Rücksprache, aber vor allem
anderen. Dabei ist völlig gleichgültig, ob er durch die letzte Änderung entstanden ist, aus einer
früheren Sitzung stammt oder schon immer da war.

„Das lag nicht an meiner Änderung" ist keine Begründung, es liegen zu lassen. Wenn geklärt werden
soll, woher ein Fehler kommt, dann als Information — nie als Grund, ihn nicht zu beheben.

## qapla-chess-gui und qapla-engine-tester laufen paarweise

qapla-chess-gui bindet qapla-engine-tester als Submodul unter `extern/qapla-engine-tester` ein.
Gleichnamige Branches gehören zusammen und laufen immer synchron:

- Branch 0.7.0 von qapla-chess-gui verweist ausschließlich auf Commits, die in
  qapla-engine-tester auf Branch 0.7.0 liegen.
- Branch master von qapla-chess-gui verweist ausschließlich auf Commits, die in
  qapla-engine-tester auf master liegen.

Was in qapla-engine-tester nur auf 0.7.0 liegt, darf nie über den Submodul-Eintrag in den master
von qapla-chess-gui gelangen. Erst wenn es in qapla-engine-tester auf master ist, darf der master
von qapla-chess-gui darauf verweisen.

## Branch immer mit Repository nennen

Jede Aussage und jede Frage zu einem Branch nennt das Repository dazu: „qapla-chess-gui, Branch
0.7.0" oder „qapla-engine-tester, Branch 0.7.0" — nie nur „0.7.0". Beide Repositories haben
gleichnamige Branches, ohne den Namen des Repositories ist die Aussage mehrdeutig.

## Synchronisierung zur GUI ausschließlich über GitHub

Ein Stand von qapla-engine-tester erreicht qapla-chess-gui nur über origin. Nie über das lokale
Dateisystem — kein `git fetch <lokaler Pfad>`, kein `git checkout FETCH_HEAD`, kein Kopieren von
Objekten in `extern/qapla-engine-tester`. Der Ablauf ist immer dieser:

1. Änderung in qapla-engine-tester committen.
2. qapla-engine-tester nach origin pushen.
3. In qapla-chess-gui im Submodul `extern/qapla-engine-tester` von origin holen und den
   gleichnamigen Branch auf diesen Stand setzen.
4. Den entstandenen Submodul-Eintrag in qapla-chess-gui committen und pushen.

Ein Commit in qapla-chess-gui darf nie auf einen Commit von qapla-engine-tester verweisen, der
noch nicht auf origin liegt. Das Submodul bleibt dabei auf seinem Branch, nicht auf detached HEAD.

## Standardablauf nach einer größeren Änderung am qapla-engine-tester

In dieser Reihenfolge, ohne Abkürzung:

1. Unit-Tests und die volle Integrations-Suite laufen lassen.
2. Erst wenn beide grün sind: committen.
3. qapla-engine-tester Branch 0.7.0 nach origin pushen.
4. **Hier anhalten und den Nutzer fragen**, ob die GUI jetzt nachgezogen werden soll.
5. Nach seinem Ja: in qapla-chess-gui im Submodul `extern/qapla-engine-tester` von origin
   pullen, den Submodul-Eintrag committen.
6. Danach GUI bauen und die UI-Tests laufen lassen.
7. Nur wenn die UI-Tests grün sind: qapla-chess-gui pushen.

Schritt 4 ist verbindlich: die UI-Tests dauern rund zehn Minuten, brauchen den Bildschirm und
öffnen ein Fenster — der Rechner muss dafür frei sein. Ohne Zusage des Nutzers wird die GUI
weder gebaut noch getestet noch gepusht.
