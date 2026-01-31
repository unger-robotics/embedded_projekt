# Kommunikationsprinzipien – Checkliste „Bloch-Standard“

*(Technische Exzellenz × Didaktische Klarheit; konsistent in „du“-Form, „wir“ nur für gemeinsame Herleitung)*

### 0) Ziel und Rahmen

**Prinzip:** Jede Passage muss gleichzeitig **prüfbar** (für Fachleute) und **nachvollziehbar** (für Einsteiger) sein.

* [ ] **Ein Thema pro Abschnitt** (keine Themenmischung).
* [ ] **Begriffe, Annahmen, Randbedingungen** stehen am Anfang (z. B. Temperatur, SoC-Fenster, Messmethode).
* [ ] **Didaktik-Form:** *Regel → Beispiel → Anwendung* (deduktiv) oder *Beobachtung → Daten → Regel* (induktiv).

---

### 1) Ansprache und Rollenbild (führend, ohne Ego)

**Prinzip:** Du führst durch Komplexität, ohne Meinungen zu „verkaufen“.

* [ ] **Aktiv statt Passiv**, wenn es um Handlungen/Schritte geht.

  * *Schlecht:* „Es wurde geprüft …“
  * *Besser:* „Wir prüfen …, indem wir … messen.“
* [ ] **„Wir“ nur als Denkprozess**, nicht als Autoritätsmarker.

  * *Besser:* „Wenn wir die Kennlinie betrachten, sehen wir …“
* [ ] **Wertungen trennen von Fakten:** erst Messwert, dann Einordnung mit Kriterium.

  * *Bsp:* „$a=4{,}2,\mathrm{m/s^2}$; das liegt über dem Referenzbereich $3{,}0\text{–}3{,}8,\mathrm{m/s^2}$.“

---

### 2) Sprache: präzise, prozessnah, ohne Behördenton

**Prinzip:** Fachbegriffe für Genauigkeit, Verben für Abläufe.

* [ ] **Verben bevorzugen**, Nominalketten vermeiden.

  * *Schlecht:* „Durchführung der Überprüfung …“
  * *Besser:* „Wir prüfen … und protokollieren …“
* [ ] **Fachbegriff beim ersten Auftreten kurz verankern** (Definition oder Kontext).

  * *Bsp:* „Rekuperation (Rückgewinnung von Bremsenergie als elektrische Energie) …“
* [ ] **Konkrete Referenzen statt „dies/jenes“** (was genau ist „dieser Wert“?).

---

### 3) Struktur im Absatz: Whiteboard-Prinzip

**Prinzip:** Leser:innen sollen den Gedankengang im Kopf „mitzeichnen“ können.

* [ ] **Mini-Dramaturgie pro Abschnitt:** *Problem → Messgröße/Modell → Schlussfolgerung → Konsequenz*.
* [ ] **Zwischenergebnisse explizit machen** (1–2 Sätze), nicht nur Endresultat.
* [ ] **Abbildungen werden geführt, nicht nur referenziert.**

  * *Statt:* „Siehe Abb. 3.“
  * *Besser:* „Im Diagramm unten erkennt man bei $80,%$ SoC den Knick; ab dort fällt die Leistung um …“

---

### 4) Zahlen, Einheiten, Vergleichsrahmen

**Prinzip:** Zahlen sind erst dann aussagekräftig, wenn Einheit, Bezug und Vergleich klar sind.

* [ ] **Jeder Messwert mit Einheit und Bezugsgröße** (z. B. pro $100,\mathrm{km}$, bei $20,^\circ\mathrm{C}$, Lastpunkt).
* [ ] **Vergleich angeben:** Referenz, Norm, Zielwert oder vorher/nachher.
* [ ] **Bandbreiten statt Punktwerte**, wenn Streuung relevant ist (Min–Max oder Mittelwert ± Streuung).

---

### 5) Anschaulichkeit mit kontrollierten Analogien

**Prinzip:** Analogien helfen, dürfen aber keine falsche Sicherheit erzeugen.

* [ ] **Analogie nur, wenn sie ein konkretes Verständnisproblem löst.**
* [ ] **Grenze der Analogie in einem Halbsatz nennen.**

  * *Bsp:* „Spannung ist *ähnlich* wie Druck und Strom wie Durchfluss – aber elektrische Felder verhalten sich nicht wie Flüssigkeiten.“

---

### 6) Evidenz, Unsicherheit, Redlichkeit

**Prinzip:** Vertrauen entsteht durch nachvollziehbare Quellenlage und klare Grenzen.

* [ ] **Quelle/Datengrundlage nennen** (Messung, Datenblatt, Paper, Norm).
* [ ] **Unsicherheit transparent machen:** Messfehler, Konfidenz, Störgrößen, Annahmen.

  * *Bsp:* „Die Aussage gilt für $5\text{–}25,^\circ\mathrm{C}$; außerhalb davon fehlen Messdaten.“
* [ ] **Keine Wertungsadjektive ohne Kriterium.**

  * *Schlecht:* „extrem gut“
  * *Besser:* „$c_w=0{,}21$; das liegt unter typischen Serienwerten von etwa $0{,}24\text{–}0{,}30$ (Vergleichsrahmen erforderlich).“

---

### 7) Stil-Hygiene (Konsistenz)

**Prinzip:** Einheitliche Konventionen reduzieren kognitive Last.

* [ ] **Einheitliche Ansprache** (du als Checkliste; „wir“ nur für Herleitung).
* [ ] **Begriffe/Abkürzungen konsistent** (SoC, $c_w$, kWh, etc.).
* [ ] **Keine Slang-Labels** (z. B. statt „Kill-List“: „Wortfilter: Adjektive durch Messkriterien ersetzen“).

---

## Review: Bloch-Standard (10 Punkte)

1. **Ein Thema?** Pro Abschnitt genau 1 Kernfrage.
2. **Ziel + Rahmen genannt?** Randbedingungen/Annahmen stehen am Anfang (z. B. Temperatur, SoC, Messaufbau).
3. **Aktiv + klarer Akteur?** Kein Passiv, außer wenn der Akteur irrelevant ist.
4. **Begriff sauber?** Jeder neue Fachbegriff ist beim ersten Auftauchen kurz verankert.
5. **Zahlen komplett?** Jeder Wert hat Einheit + Bezugsgröße (z. B. pro $100,\mathrm{km}$).
6. **Vergleich vorhanden?** Referenz/Ziel/Norm oder Vorher-Nachher genannt.
7. **Herleitung sichtbar?** Mindestens 1 Zwischenschritt: Problem → Daten/Modell → Schluss → Konsequenz.
8. **Unsicherheit ehrlich?** Streuung/Bandbreite oder Messfehler/Limitierung genannt, wenn relevant.
9. **Wertungen eliminiert?** Keine „gut/schlecht/krass“ ohne Kriterium; Einordnung nur mit Maßstab.
10. **Konsistenz geprüft?** Ansprache (du/wir), Abkürzungen, Notation, Schreibweise überall gleich.

