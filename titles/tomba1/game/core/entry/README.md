# Application composition owner

`main.cpp` installs the Tomba! 1 runtime and generated registry, constructs the shared psxport machine,
loads the provisioned executable, composes platform devices, and enters the native-owned frame shell.
It does not implement engine, renderer, input, audio, configuration, or RE logic.

Startup requires `PSXPORT_TOMBA1_DISC` before constructing the machine. Generic `PSXPORT_DISC` and
drop-in discovery are intentionally refused here: those framework-wide fallbacks cannot prove that
the media belongs to this title and previously let another game's disc enter the Tomba! 1 runtime.
The provisioner remains the authority that verifies the selected disc and extracts the executable.
`-h` and `--help` are handled before those checks and exit successfully without requiring either
asset; the CTest contract runs both spellings from an asset-free working directory.
The repository launcher selects this owner as `./run.sh tomba1`; its own `-h`/`--help` dispatch is
also resolved before host dependencies, framework synchronization, provisioning, or disc discovery.

This follows another game project's layout's host-entry shape: lifecycle composition is separate from the subsystem owners
it connects. The product target was published only after the authenticated executable/overlay evidence boundary agreed.

The entry point must not parse, register, or forward a 60fps or native-rendering option. Tomba! 1 has
no such compatibility modes; its only title enhancement control is the future widescreen policy.
