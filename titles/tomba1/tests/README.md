# Tests

`test_stream_field_turn.cpp` proves that one title stream field advances logic-only SPU/XA exactly
once before it pumps the controller, then drives the shipping callback through both refusal gates:
no direct-runtime callback layout and no active continuous stream. Both leave sector delivery
unchanged; real-disc execution remains the evidence that an active stream advances guest code.

`test_widescreen_projection.cpp` covers the title's widescreen projection. `test_guest_task_budget_resume.cpp`
covers the bounded resume of a guest-task budget exit across display fields, and the guest spin it
refuses; both halves run real guest code on the real dynarec over real display-field budgets and need
no provisioned disc.

These hermetic tests do not substitute for the required bounded product launch, visible output, input
response, or widescreen evidence.