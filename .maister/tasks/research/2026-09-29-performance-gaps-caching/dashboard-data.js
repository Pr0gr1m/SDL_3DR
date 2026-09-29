window.MAISTER_DATA = {
  generated: "2026-09-29T17:52:36Z",
  task: { title: "Performance gaps, especially caching", type: "research", status: "completed", description: "analyze performance gaps, especially related to caching", path: ".maister/tasks/research/2026-09-29-performance-gaps-caching", current_activity: null },
  characteristics: {},
  phases: [
    { id: "phase-1", name: "Research foundation", icon_hint: "analysis", status: "completed", started: "2026-09-29T17:22:43Z", completed: "2026-09-29T17:33:00Z", skip_reason: null,
      summary: "Only cache (per-chunk CPU Face list) is broken and shallow; full scene re-flattened and re-uploaded every frame; nothing profiled.",
      decisions: [{ decision: "Measure before changing code", rationale: "nothing profiled; Debug inflates STL costs" }],
      risks: ["Build type behind FPS claims unknown (likely Debug)", "Struct sizes unverified estimates"],
      artifacts: [{ path: "outputs/research-report.md", label: "Research report", html: "outputs/research-report.html" }, { path: "analysis/synthesis.md", label: "Synthesis", html: null }],
      gate: { question: "Continue to brainstorming evaluation?", answer: "Continue" } },
    { id: "phase-2", name: "Evaluate brainstorming value", icon_hint: "plan", status: "completed", started: "2026-09-29T17:33:00Z", completed: "2026-09-29T17:33:30Z", skip_reason: null, summary: "Brainstorming and design both enabled.", decisions: [], risks: [], artifacts: [], gate: null },
    { id: "phase-3", name: "Generate solution alternatives", icon_hint: "spec", status: "completed", started: "2026-09-29T17:33:30Z", completed: "2026-09-29T17:37:00Z", skip_reason: null, summary: "20 alternatives across 5 decision areas.", decisions: [], risks: ["If Release shows GPU-bound or already fast, stop after measurement plus bug fixes", "Cross-chunk culling needs a policy for unloaded neighbors"],
      artifacts: [{ path: "outputs/solution-exploration.md", label: "Solution exploration", html: "outputs/solution-exploration.html" }], gate: null },
    { id: "phase-4", name: "Evaluate alternatives", icon_hint: "plan", status: "completed", started: "2026-09-29T17:37:00Z", completed: "2026-09-29T17:48:00Z", skip_reason: null,
      summary: "User converged on all 5 decision areas.",
      decisions: [
        { decision: "Area 1: 1B without build-config change", rationale: "in-app per-stage timers and benchmark path; no Release preset" },
        { decision: "Area 2: 2B dirty-flag chunk mesh", rationale: "removes stale/camera-dependent cache by construction" },
        { decision: "Area 3: 3B per-chunk persistent GPU buffers", rationale: "static chunks cost zero CPU mesh/upload work" },
        { decision: "Area 4: 4B neighbor-aware cross-chunk culling", rationale: "removes interior hidden faces, smaller uploads" },
        { decision: "Area 5: 5A keep optional<Object>", rationale: "zero risk; revisit when a measured trigger appears" }
      ],
      risks: ["Timings without a Release build will be inflated; use for relative stage ranking only"], artifacts: [], gate: { question: "Continue to high-level design?", answer: "Continue" } },
    { id: "phase-5", name: "High-level design", icon_hint: "spec", status: "completed", started: "2026-09-29T17:48:00Z", completed: "2026-09-29T17:52:36Z", skip_reason: null, summary: "Dirty-flag per-chunk GPU meshes; 5 ADRs; staged order measure, dirty flag, GPU buffers, neighbor culling; dependency-free.", decisions: [{ decision: "Dirty flag replaces the Face cache", rationale: "removes stale/camera-dependent bug class by construction" }, { decision: "Per-chunk persistent GPU buffers with pooled transfer buffer", rationale: "static chunks cost zero CPU mesh/upload work" }], risks: ["Thread safety of E-key edits vs parallel meshing unverified", "Deferred release of in-flight GPU buffers needed", "Unloaded-neighbor boundary policy must be stated"], artifacts: [{ path: "outputs/high-level-design.md", label: "High-level design", html: "outputs/high-level-design.html" }, { path: "outputs/decision-log.md", label: "Decision log", html: "outputs/decision-log.html" }], gate: { question: "Design complete. Continue to output generation?", answer: "Continue" } },
    { id: "phase-6", name: "Completion", icon_hint: "done", status: "completed", started: "2026-09-29T17:52:36Z", completed: "2026-09-29T17:52:36Z", skip_reason: null, summary: null, decisions: [], risks: [], artifacts: [], gate: null }
  ],
  verification: { status: null, issues: [], fixes: [], reverify_count: 0 }
};
