/* Help > About: hello's `about` text (core/xpp_about.cpp) split into lines,
   each a run of plain text and URLs, so the view can make the URLs links
   without parsing anything the core did not say. Pure, no DOM. */
export type AboutPart = {text: string; url?: string};

const URL_RE = /https?:\/\/[^\s]+/g;

export function aboutLines(text: string): AboutPart[][] {
  return text.split('\n').map(line => {
    const parts: AboutPart[] = [];
    let at = 0;
    for (const m of line.matchAll(URL_RE)) {
      if (m.index > at) parts.push({text: line.slice(at, m.index)});
      parts.push({text: m[0], url: m[0]});
      at = m.index + m[0].length;
    }
    if (at < line.length || parts.length === 0) parts.push({text: line.slice(at)});
    return parts;
  });
}
