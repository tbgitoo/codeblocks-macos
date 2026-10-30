# ANSI colour sequences in the Program Console

## Observation

Some programs, including GTK/GDK applications, emit ANSI terminal colour sequences such as `ESC[1;33m` and `ESC[0m`. The Code::Blocks Program Console currently displays these sequences literally instead of interpreting them as colours.

## Possible improvement

Investigate lightweight ANSI SGR handling in the Program Console so common foreground colours, bold/reset sequences, and related formatting render correctly rather than appearing as text.

## Boundary

- Keep this independent of GTK and other individual libraries.
- Start with common SGR colour/reset sequences only.
- Preserve plain-text output and avoid turning the Program Console into a full terminal emulator.
- If colour rendering is not practical, strip recognised SGR sequences cleanly as a fallback.

## Acceptance idea

A program printing coloured warnings shows readable coloured text, or at minimum clean uncoloured text, without visible `ESC[...]` fragments.
