# LaTeX-Konvertierung und Qualitätsprüfung

## Prompt für neue Dateien (Markdown → LaTeX)

```
Erzeuge LaTeX-Fragmente **ohne Präambel**, passend zur main_*.tex und techdoc.cls.

**Anforderungen:**
1. **Unicode-Prüfung:** Ersetze kritische Zeichen (U+2500 Boxzeichen, fehlende Pfeile)
2. **Mathematik:** Formeln in LaTeX-Syntax ($...$, \[...\], align)
3. **Einheiten:** \SI{}{} und \SIrange{}{}{} aus siunitx
4. **Tabellen:** booktabs + tabularx (\toprule, \midrule, \bottomrule)
5. **Code:** lstlisting mit Styles: arduino, python, shell, json
6. **Boxen:** infobox, warnbox, tipbox für Hinweise
7. **Referenzen:** \cref{} für Querverweise, \label{} für alle Elemente
```

---

## Prompt für bestehende .tex-Dateien (Prüfung)

```
Identifiziere Schriftart-Kompatibilitätsprobleme in der hochgeladenen .tex-Datei.

**Prüfe auf:**
1. **Kritische Unicode-Zeichen:**
   - U+2500-257F (Box Drawing: ─│┌┐└┘├┤┬┴┼)
   - U+2190-21FF (Pfeile: ↑↓↔ – falls nicht in cls definiert)
   - U+2600-26FF (Symbole: ☐☑☒★☆●○)

2. **Erlaubte Zeichen (in techdoc.cls definiert):**
   - → (U+2192), ← (U+2190), ↔ (U+2194)
   - ✓ (U+2713), ✗ (U+2717)
   - Deutsche Umlaute (äöüÄÖÜß)
   - En-Dash (–), Em-Dash (—)

3. **Mathematik/Einheiten prüfen:**
   - Ω → $\Omega$ oder \SI{}{\ohm}
   - µ → \SI{}{\micro...} oder $\mu$
   - ° → \SI{...}{\degree} oder $^\circ$
   - × → $\times$ oder \num{...e...}

**Ausgabe:** Tabelle mit Zeile, Problem, Korrektur
```

---

## Schnellreferenz: techdoc.cls-Konventionen

### Definierte Unicode-Zeichen
```latex
\newunicodechar{→}{$\rightarrow$}
\newunicodechar{←}{$\leftarrow$}
\newunicodechar{↔}{$\leftrightarrow$}
\newunicodechar{✓}{$\checkmark$}
\newunicodechar{✗}{$\times$}
```

### Einheiten (siunitx)
```latex
\SI{10}{\milli\second}        % 10 ms
\SI{3.3}{\volt}               % 3,3 V
\SI{921600}{\baud}            % 921600 Baud
\SI{10}{\kilo\ohm}            % 10 kΩ
\SIrange{20}{50}{\micro\second}  % 20–50 µs
\num{921600}                  % 921 600
```

### Listings
```latex
\begin{lstlisting}[style=arduino, caption={...}, label={lst:...}]
\begin{lstlisting}[style=python]
\begin{lstlisting}[style=shell]
\begin{lstlisting}[style=json]
\begin{lstlisting}[style=wrap]  % für lange Ausgaben
```

### Tabellen
```latex
\begin{table}[H]
\centering
\caption{...}
\label{tab:...}
\begin{tabularx}{\textwidth}{l X}  % oder {l l l}, {c c c}
\toprule
\textbf{Spalte 1} & \textbf{Spalte 2} \\
\midrule
Inhalt & Inhalt \\
\bottomrule
\end{tabularx}
\end{table}
```

### Boxen
```latex
\begin{infobox}[Titel]...\end{infobox}   % Teal, Info
\begin{warnbox}[Titel]...\end{warnbox}   % Orange, Warnung
\begin{tipbox}[Titel]...\end{tipbox}     % Grün, Tipp
```

### Makros
```latex
\code{inline_code}           % \texttt{}
\highlight{wichtig}          % Teal + Bold
\warning{Achtung}            % Orange + Bold
\critical{Kritisch}          % Rot + Bold
\zB \dhei \ua \vgl \bzw \ggf \evtl  % Abkürzungen mit Spacing
```

### Häufige Ersetzungen

| Markdown/Unicode | LaTeX                         |
|------------------|-------------------------------|
| `──` (Box)       | `--` oder `---`               |
| `↑↓`             | `$\uparrow\downarrow$`        |
| `Ω`              | `$\Omega$` oder `\SI{}{\ohm}` |
| `µs`             | `\SI{}{\micro\second}`        |
| `°C`             | `\SI{}{\degreeCelsius}`       |
| `±`              | `$\pm$`                       |
| `≈`              | `$\approx$`                   |
| `≤` `≥`          | `$\leq$` `$\geq$`             |
| `"text"`         | `\glqq text\grqq`             |
| `->` in Text     | `$\rightarrow$` oder `→`      |

