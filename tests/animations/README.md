# W520 animation conversion fixtures

`imports.tsv` maps the seven retained foreign inputs to their models.
`test_anix` compares their native conversion with the original frame data
at five states, including permanent shapes, trails and grab assignments.
`examples.tsv` maps all 32 native examples to their models.

Three original examples had invalid grab tolerances: `acoaster` and
`coaster2D` wrote `tol` (not a parameter); `ielaspen` wrote `.02*scale`.
XPPAUT read these as .02 through its numeric-prefix/default rule. Their
native examples explicitly retain that value; the importer still refuses
invalid numeric fields. See findings 92–93 and docs/odex-quirks.md.
