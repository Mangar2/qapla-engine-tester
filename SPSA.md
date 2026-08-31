# Tuning Engine Parameters with SPSA

A practical guide to `--spsa`: what can be tuned, how to choose the parameters and step
sizes, how to set the run up in a settings file, and how to read the result.

- [1. What SPSA can tune](#1-what-spsa-can-tune)
- [2. What SPSA does](#2-what-spsa-does)
- [3. Choosing the parameters](#3-choosing-the-parameters)
- [4. Choosing the numbers](#4-choosing-the-numbers)
- [5. Setting the run up in a settings file](#5-setting-the-run-up-in-a-settings-file)
- [6. Overriding single values on the command line](#6-overriding-single-values-on-the-command-line)
- [7. Running it and reading the output](#7-running-it-and-reading-the-output)
- [8. After the run: confirm with SPRT](#8-after-the-run-confirm-with-sprt)
- [9. Troubleshooting](#9-troubleshooting)
- [10. SPSA or CLOP?](#10-spsa-or-clop)

---

## 1. What SPSA can tune

Start here, because it decides whether your parameter is a candidate at all.

**Only UCI options that carry an integer value can be optimized.** The optimizer works
internally with real numbers, but every value it wants to try is rounded and handed to the
engine as a plain integer:

```
setoption name Contempt value 17
```

That gives three hard requirements:

1. **The engine speaks UCI.** WinBoard/XBoard engines are fully supported for playing games,
   but they have no equivalent of `setoption`, so they cannot be tuned.
2. **The option is an integer option** (`type spin` in the engine's `uci` output). A
   `check`, `combo`, `string` or `button` option has no gradient to follow and cannot be
   used. A parameter that is conceptually a fraction has to be exposed *scaled* — publish
   `LMRFactorPercent` with a range of `50..200` instead of a float `0.5..2.0`, and divide
   inside the engine.
3. **The option must be settable between games.** Every iteration starts fresh engine
   processes and sends the options before the games, so anything the engine reads at startup
   is fine — but an option the engine ignores after initialization will not be tuned.

**The name must match the engine's own spelling.** Qapla Engine Tester starts the engine once
before the optimization begins and compares your `min`/`max` against the bounds the engine
reports:

- The configured range is **not covered** by the engine's declared `min`/`max` → the run is
  rejected with `InvalidParameters`. Widen the option in the engine, or narrow the range here.
- The engine **does not report the option at all** → you only get a *warning*, and the run
  starts. This is the dangerous case: the engine silently discards the unknown option, both
  sides of every pair play identically, and you spend a night of CPU time tuning pure noise.
  Read that warning.

Before the first real run, look at what the engine actually offers:

```bash
echo -e "uci\nquit" | ./my-engine | grep "option name"
```

and take `name`, `min` and `max` from there.

---

## 2. What SPSA does

*Simultaneous Perturbation Stochastic Approximation* is a gradient method for a function you
can only measure with a lot of noise — here: playing strength as a function of the parameter
vector θ.

One **iteration** does this:

1. Draw a random direction Δ: independently `+1` or `-1` for *every* parameter at once
   (that is the "simultaneous perturbation").
2. Build two versions of your engine: one with `θ + c·Δ`, one with `θ − c·Δ`, where `c` is
   the per-parameter `step`.
3. Let those two versions play `gamesperpair` games against each other, colours alternating.
4. Move the parameters towards the winner:

   ```
   θ  +=  learningrate · c · (wins − losses) · Δ
   ```

   and clamp each parameter into its `min`/`max` range.

Two things follow from this, and they shape everything below:

**One iteration carries almost no information.** With 8 games, `wins − losses` is typically
±2 even when both sides are exactly equally strong. A single iteration is mostly noise; the
signal only emerges because thousands of iterations are averaged by the update rule. With the
defaults (`iterations=20000`, `gamesperpair=8`) a full run is **160,000 games** — SPSA is a
procedure for very short time controls and long, unattended runs.

**All parameters move on every iteration.** SPSA never measures a single parameter in
isolation; it gets one scalar per iteration (the game result) and distributes it over all
dimensions. That is what makes it cheap in high dimensions — and what makes the choice of
parameters the critical decision.

---

## 3. Choosing the parameters

### Use few of them

Each additional parameter dilutes the same one-number-per-iteration signal. The result at a
fixed number of games gets worse with every dimension you add, and it gets worse quickly.

| Parameters | Expectation |
| --- | --- |
| 1–3 | Comfortable. Converges within a realistic number of games. |
| 4–8 | Workable if the parameters are genuinely independent and you can afford 100k+ games. |
| more | Expect a random walk unless every single parameter has a large individual effect. |

Tuning four parameters well beats tuning twenty badly. If you have twenty candidates, run
several small tunings in sequence rather than one large one.

### Keep them independent

This is the point that decides most runs, and it is the same requirement CLOP has (see
[CLOP in the README](README.md#-clop--confident-local-optimization)): the optimizer fits *one*
model over all parameters simultaneously, and it can only separate their effects if they
*have* separable effects.

Two parameters are a problem when they push on the same behaviour:

- Two pruning margins that both simply make the search more aggressive. Increasing one is
  almost the same experiment as increasing the other.
- A depth threshold and a reduction amount for the same heuristic. Their product is what
  matters, not either value.
- A bonus and the divisor that later scales it. Only the ratio is observable.

For such a pair the strength surface has a long flat valley: many combinations play equally
well. SPSA then drifts *along* the valley for thousands of iterations without gaining
anything, and the two parameters end up somewhere arbitrary on that valley floor — often at
values that look surprisingly far from their defaults while being worth exactly zero Elo.

Ways out, in order of preference:

1. **Tune one, hold the other fixed.** The simplest and usually the best answer.
2. **Reparameterize.** Instead of two absolute values, expose a total and a share
   (`TotalMargin` and `MarginSplitPercent`), or a base and a factor. These are far less
   correlated than the two raw numbers, and each has a meaning the optimizer can find.
3. **Split into separate runs**, one per mechanism.

### Make sure the parameter matters

A parameter that changes almost no games at your time control cannot be tuned, no matter how
many games you play. Before spending CPU on it, check that a clearly different value produces
a clearly different playing style, or a measurable strength change. Parameters that only
become active at high depth are poor candidates for the ultra-fast time controls SPSA needs.

Also remember that SPSA tunes against *the engine itself*. It finds what is best in self-play,
which is not automatically what is best against foreign opponents — one more reason for the
verification run in [section 8](#8-after-the-run-confirm-with-sprt).

---

## 4. Choosing the numbers

### `default`, `min`, `max`

`default` is where the optimization starts — use the engine's current value, so that a run
that finds nothing costs you nothing. `min`/`max` should contain only values that are
*plausible* and let the engine play reasonable chess; the optimizer clamps to them, and a
range that reaches into nonsense wastes games there. Keep the default roughly in the middle,
and stay inside the bounds the engine itself declares for the option.

### `step` (the perturbation `c`)

The single most important number after the parameter choice. `step` is how far the two sides
of a pair are moved apart, so it decides whether the pair result contains any information at
all.

- **Rule of thumb: 5–10 % of the `max − min` range.**
- **Too small** and `θ+c` plays exactly like `θ−c`. The result is a coin flip, the update is
  a random walk, and the run tells you nothing.
- **Too large** and both sides play badly. You then measure which kind of damage is worse,
  not the local slope at your current value.

If you are unsure, err on the larger side: a step that is somewhat too big still points in the
right direction, while one that is too small produces noise.

### `learningrate`

The step size of the update. It only ever appears multiplied by `step`, so the actual movement
per iteration is `learningrate · step · (wins − losses)`. With the default `0.002` and a
typical margin of two games, one iteration moves a parameter by about `0.004 · step` — it
takes a few hundred consistent iterations to shift a parameter by one `step`. That slowness is
deliberate: it is what averages the noise away. Raise it only if you also raise
`gamesperpair`, and expect a noisier, more jittery trajectory in return.

### `gamesperpair`, `iterations`, `activepairs`

- `gamesperpair` (default 8) — games behind each single gradient sample. More games mean a
  cleaner sign per iteration but fewer iterations for the same total. Below ~4 the sign is
  nearly random; above ~16 you are better off spending the games on more iterations. Each pair
  also starts two engine processes, so very small values pay the startup cost too often.
- `iterations` (default 20000) — total games are `iterations × gamesperpair`. Choose the time
  control so that this number of games actually fits into the time you have.
- `activepairs` (default 32) — how many pairs are in flight at once. The games of one pair run
  sequentially, so this should be **at least your `concurrency`**, otherwise cores sit idle.
  Higher values are harmless; they only mean the updates from a few more iterations arrive
  slightly later.

### Time control and throughput

Pick the fastest time control at which your engine still behaves correctly — something like
`tc=5+0.05` — and enable `--rapid` to suppress `info` line processing. Turn PGN output off, or
keep it minimal: 160,000 games produce a very large file that nobody will read.
`--systemtest` tells you how many parallel games your machine sustains before the results are
distorted by scheduling; use that number for `concurrency`.

---

## 5. Setting the run up in a settings file

An SPSA run has many settings and gets repeated with small variations, so **put it into an
INI settings file** rather than into a shell line. The file documents the run, it can be
committed next to the engine, and the parts you actually vary can still be overridden per run
on the command line (see [section 6](#6-overriding-single-values-on-the-command-line)).

Section headers are the CLI groups (`--spsa` → `[spsa]`), keys are the sub-parameters, and
global options stand at the top without a section. Groups that may appear several times —
notably `[spsavalue]` — are simply repeated.

`spsa-lmr.ini`:

```ini
; Global options
concurrency=8
rapid=true

[engine]
name=Qapla-tune
cmd=C:\Chess\engines\qapla.exe

[each]
proto=uci
tc=5+0.05

[openings]
file=C:\Chess\books\8ply-uniform.epd

[spsa]
iterations=20000
gamesperpair=8
activepairs=32
learningrate=0.002
outcomeinterval=50
seed=42

[spsavalue]
name=LMRBaseReduction
default=100
min=50
max=200
step=10

[spsavalue]
name=NullMoveReduction
default=300
min=200
max=500
step=25

[logging]
engine=false
path=log
```

Run it with:

```bash
qapla-engine-tester --settingsfile=spsa-lmr.ini
```

Notes on the file above:

- **One engine is enough.** SPSA optimizes the first configured engine and creates the two
  perturbed opponents from it automatically — you never define the opponent yourself. (Any
  further engines are ignored by the SPSA run.)
- **An openings file is mandatory**, and a time control must be set. SPSA picks a random start
  position for each iteration itself; `seed` makes that choice — and therefore the whole
  run — reproducible.
- **Every field of `[spsavalue]` is mandatory**: `name`, `default`, `min`, `max`, `step`.
- Engines can just as well come from an engines file (`enginesfile=engines.ini` plus
  `[engine] conf=Qapla-tune`), which is the better arrangement once you tune several engines.

---

## 6. Overriding single values on the command line

Everything in the settings file is also available as a CLI parameter, in the same
group/key form:

```bash
qapla-engine-tester --concurrency=8 --rapid \
  --engine name=Qapla-tune cmd=/opt/engines/qapla \
  --each proto=uci tc=5+0.05 \
  --openings file=books/8ply-uniform.epd \
  --spsa iterations=20000 gamesperpair=8 learningrate=0.002 \
  --spsavalue name="LMRBaseReduction" default=100 min=50 max=200 step=10 \
  --spsavalue name="NullMoveReduction" default=300 min=200 max=500 step=25
```

**Command line first.** When the same setting appears in both places, the command line wins —
so the recommended way to work is: keep the whole run in the `.ini`, and override only what
you vary:

```bash
# same configuration, but a different seed and a shorter probe run
qapla-engine-tester --settingsfile=spsa-lmr.ini --spsa iterations=200 seed=7

# same configuration on a bigger machine
qapla-engine-tester --settingsfile=spsa-lmr.ini --concurrency=32
```

Overriding works per key: `--spsa iterations=200 seed=7` replaces exactly those two values and
leaves `gamesperpair`, `learningrate` and the rest of the `[spsa]` section untouched. The full
precedence order (result files > command line > settings file > engines file) is described in
[Configuration Precedence](README.md#-configuration-precedence).

A short probe run like the one above — 200 iterations — is worth doing before every long run.
It shows the startup warnings, the parameter table and the games-per-minute rate, which is all
you need to see whether the option names are right and whether the full run fits into the
night.

---

## 7. Running it and reading the output

Every `outcomeinterval` completed iterations, and at the end, the optimizer prints its
progress and a parameter table:

```
Parameter            Initial     Value      Mean2k     Mean5k   RelStd2k%  RelStd5k%     Min       Max
LMRBaseReduction         100     118.4       115.2      109.7        4.1        9.6       50       200
NullMoveReduction        300     287.1       291.6      296.3        2.2        5.1      200       500
```

How to read it:

- **`Value`** is the current θ — a single point on a noisy trajectory. **Do not take your new
  default from this column.**
- **`Mean2k` / `Mean5k`** are the averages over the last 2000 / 5000 iterations. *This* is the
  result of the run. If the two agree, the parameter has settled; if `Mean2k` is still moving
  away from `Mean5k`, the run is not finished.
- **`RelStd2k%` / `RelStd5k%`** show how much the parameter still moves, relative to its
  starting value. Small and shrinking means converged. Large and constant means the parameter
  is wandering — usually too small a `step`, an ineffective parameter, or a correlated pair
  as described in [section 3](#3-choosing-the-parameters).
- A value that has crept to `Min` or `Max` and stays there means your range was too narrow, or
  that the parameter simply wants to be turned off. Widen the range and rerun before believing
  the bound itself.

With `--interactive` you can ask for this table at any time, and stop the run gracefully.
Note that an SPSA run has **no result file and cannot be resumed** — if you stop it, keep the
log, because the last printed table is your result.

---

## 8. After the run: confirm with SPRT

SPSA gives you a *candidate*, not a verdict. It tuned in self-play, at one very fast time
control, on one opening set, and it will happily report a shift for a parameter that is worth
nothing. Always verify:

1. Set the parameter to the rounded `Mean5k` value in the engine (or via `option.<name>` in
   the engine configuration).
2. Run an [SPRT](README.md#-sprt--sequential-probability-ratio-test) of the new value against
   the old default, at the time control you actually care about, with an opening book the
   tuning did not use.
3. Only adopt the value if that test confirms it.

A tuning that survives this step is real. One that does not has told you something valuable
too: that this parameter is not where the strength is.

---

## 9. Troubleshooting

| Symptom | Likely cause |
| --- | --- |
| Run stops with `the configured optimization range is not covered by the option bounds` | Your `min`/`max` is wider than what the engine declares for the option. Narrow it, or widen the option in the engine. |
| Warning `engine does not support the option` | The name does not match the engine's `uci` output. The engine ignores it, and the run measures nothing. Fix the name. |
| `No engine defined for SPSA optimization` | No `[engine]` section / `--engine`, or the engine could not be started. |
| `SPSA requires an openings file` | No `[openings] file=...` configured. |
| `SPSA requires at least one parameter to optimize` | No `[spsavalue]` section, or one of its mandatory fields is missing. |
| Parameters wander without settling, `RelStd` stays large | `step` too small, parameter has too little effect, or two parameters are correlated. |
| A parameter jumps to a bound and stays there | Range too narrow, or `step`/`learningrate` too large. |
| Cores idle | `activepairs` lower than `concurrency`. |
| Progress far slower than expected | Time control too slow for `iterations × gamesperpair` games; enable `--rapid` and disable PGN output. |

---

## 10. SPSA or CLOP?

Both optimizers tune integer UCI options and both are in this tester, but they answer
different questions:

- **CLOP** (`--clop`, see [the README section](README.md#-clop--confident-local-optimization))
  fits a local quadratic win model and samples where it expects to learn the most. For **one
  to three** parameters with a genuine optimum it usually finds the value in fewer games, and
  it gives a confidence statement about the position of that optimum.
- **SPSA** (this guide) needs no model. It scales to **a handful** of parameters and to very
  large numbers of very fast games, and it is the better choice when you expect a gentle slope
  rather than a sharp optimum.

Both share the requirements from [section 3](#3-choosing-the-parameters): few parameters,
independent from each other, each with a real effect on play. Neither can repair a badly
chosen parameter set.

---

**See also:** [PARAMETERS.md](PARAMETERS.md#--spsa) for the complete `--spsa` and
`--spsavalue` reference, and the [README](README.md) for the surrounding options
(openings, PGN output, logging, concurrency).
