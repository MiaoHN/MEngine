/*
 * Render ```mermaid fences.
 *
 * Why this file exists: Material for MkDocs has a built-in mermaid flow, but it
 * hooks on `pre.mermaid` and (verified with Material 9.7.7) replaced the block
 * with an EMPTY `<div class="mermaid">` - the diagram source was lost and
 * nothing was rendered. Its built-in flow also loads mermaid from an unpinned
 * unpkg URL at runtime.
 *
 * So the fences use a private class (`mermaid-diagram`, see mkdocs.yml) that the
 * theme ignores, and this script renders them with a version-pinned mermaid. It
 * handles Material's instant navigation (content is swapped without a reload)
 * and palette switches (light/dark) by watching the DOM.
 */
(() => {
  const MERMAID_URL = "https://cdn.jsdelivr.net/npm/mermaid@11.4.1/dist/mermaid.esm.min.mjs";
  const DIAGRAM_SELECTOR = "pre.mermaid-diagram";

  let mermaidPromise = null;
  let lastScheme = null;

  const loadMermaid = () => {
    if (!mermaidPromise) {
      // Reuse the copy Material may already have fetched, otherwise import a
      // pinned ESM build.
      mermaidPromise = window.mermaid
        ? Promise.resolve(window.mermaid)
        : import(/* @vite-ignore */ MERMAID_URL).then((mod) => mod.default ?? mod.mermaid);
    }
    return mermaidPromise;
  };

  const currentScheme = () =>
    document.body.getAttribute("data-md-color-scheme") === "slate" ? "dark" : "default";

  const render = async () => {
    const scheme = currentScheme();
    // A palette switch needs a re-render: the diagram colours are baked in.
    const pending = Array.from(document.querySelectorAll(DIAGRAM_SELECTOR)).filter(
      (node) => node.dataset.mdDiagram !== scheme,
    );
    if (pending.length === 0) {
      lastScheme = scheme;
      return;
    }

    const mermaid = await loadMermaid();
    if (mermaid.initialize) {
      mermaid.initialize({
        startOnLoad: false,
        theme: scheme === "dark" ? "dark" : "default",
        securityLevel: "loose", // diagrams in this repo contain inline HTML-ish labels
        flowchart: { htmlLabels: true, useMaxWidth: true },
      });
    }

    for (const node of pending) {
      // Keep the diagram source: after the first render `textContent` is the
      // generated SVG, so a re-render (palette switch) must reuse the original.
      if (!node.dataset.mdSource) {
        node.dataset.mdSource = node.textContent;
      }
      try {
        const { svg } = await mermaid.render(`md-mermaid-${Math.random().toString(36).slice(2)}`, node.dataset.mdSource);
        node.innerHTML = svg;
        node.dataset.mdDiagram = scheme;
        node.classList.add("mermaid-diagram--rendered");
        node.classList.remove("mermaid-diagram--error");
      } catch (error) {
        // Leave the source visible instead of an empty box; the build-time
        // `mkdocs build --strict` cannot catch client-side diagram errors.
        console.error("[mermaid] failed to render diagram", error);
        node.textContent = node.dataset.mdSource;
        node.dataset.mdDiagram = scheme;
        node.classList.remove("mermaid-diagram--rendered");
        node.classList.add("mermaid-diagram--error");
      }
    }
    lastScheme = scheme;
  };

  let timer = null;
  const schedule = () => {
    if (timer !== null) {
      clearTimeout(timer);
    }
    timer = setTimeout(() => {
      timer = null;
      render();
    }, 60);
  };

  // Instant navigation replaces the article, and the palette toggle rewrites
  // <body> attributes, so watch the DOM instead of relying on load events.
  const start = () => {
    new MutationObserver(schedule).observe(document.body, {
      childList: true,
      subtree: true,
      attributes: true,
      attributeFilter: ["data-md-color-scheme"],
    });
    render();
  };

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", start);
  } else {
    start();
  }
})();
