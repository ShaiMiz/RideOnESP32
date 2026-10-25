import React, { useMemo, useState } from 'react';

const STATUS_CLASS = (s = '') => {
  const t = s.toLowerCase();
  if (t.startsWith('tbd')) return 'tbd';
  if (t.startsWith('to buy')) return 'buy';
  if (t.startsWith('ordered')) return 'ordered';
  if (t.startsWith('owned')) return 'owned';
  if (t.startsWith('stock')) return 'stock';
  return 'other';
};

export function StatusChip({ status }) {
  if (!status) return null;
  return <span className={`chip ${STATUS_CLASS(status)}`}>{status.replace(/ 24 Oct$/, '')}</span>;
}

function Eye({ on, partial, onClick, title }) {
  return (
    <button className={`eye ${on ? 'on' : 'off'} ${partial ? 'partial' : ''}`} onClick={onClick} title={title} aria-label={title}>
      {on ? (
        <svg viewBox="0 0 24 24" width="15" height="15"><path d="M12 5C6.5 5 2.7 9.4 1.5 12c1.2 2.6 5 7 10.5 7s9.3-4.4 10.5-7C21.3 9.4 17.5 5 12 5zm0 11a4 4 0 110-8 4 4 0 010 8z" fill="currentColor" /></svg>
      ) : (
        <svg viewBox="0 0 24 24" width="15" height="15"><path d="M3 4.3L4.3 3 21 19.7 19.7 21l-3.2-3.2c-1.4.7-2.9 1.2-4.5 1.2-5.5 0-9.3-4.4-10.5-7 .7-1.5 2.1-3.4 4.1-4.9L3 4.3zM12 5c5.5 0 9.3 4.4 10.5 7-.5 1.1-1.4 2.5-2.7 3.8L16 12a4 4 0 00-4-4L9.3 5.4C10.2 5.1 11.1 5 12 5z" fill="currentColor" /></svg>
      )}
    </button>
  );
}

// Component tree: assemblies with their parts, one eye per assembly and a checkbox per part.
export default function Sidebar({ model, hidden, selected, onToggleParts, onSelect, onShowAll, onHideAll, onIsolate }) {
  const [query, setQuery] = useState('');
  const [open, setOpen] = useState(() => new Set(['box', 'box.drive', 'wheels']));
  const tree = useMemo(() => {
    const kids = {};
    for (const a of model.assemblies) (kids[a.parent || ''] ||= []).push(a);
    const parts = {};
    for (const p of model.parts) (parts[p.assembly] ||= []).push(p);
    return { kids, parts };
  }, [model]);

  const q = query.trim().toLowerCase();
  const matches = (p) => !q || `${p.id} ${p.name} ${p.type} ${p.status}`.toLowerCase().includes(q);
  const partsUnder = (id) => [...(tree.parts[id] || []), ...(tree.kids[id] || []).flatMap((a) => partsUnder(a.id))];

  const node = (a, depth) => {
    const all = partsUnder(a.id);
    if (!all.length) return null;
    const shownParts = (tree.parts[a.id] || []).filter(matches);
    const childNodes = (tree.kids[a.id] || []).map((c) => node(c, depth + 1)).filter(Boolean);
    if (q && !shownParts.length && !childNodes.length) return null;
    const visibleCount = all.filter((p) => !hidden.has(p.id)).length;
    const isOpen = open.has(a.id) || !!q;
    return (
      <li key={a.id} className="tree-assembly">
        <div className="row assembly" style={{ paddingLeft: 6 + depth * 12 }}>
          <button className="caret" onClick={() => setOpen((s) => { const n = new Set(s); n.has(a.id) ? n.delete(a.id) : n.add(a.id); return n; })} aria-label={isOpen ? 'Collapse' : 'Expand'}>{isOpen ? '▾' : '▸'}</button>
          <Eye on={visibleCount > 0} partial={visibleCount > 0 && visibleCount < all.length} title={visibleCount ? `Hide ${a.name}` : `Show ${a.name}`} onClick={() => onToggleParts(all.map((p) => p.id), visibleCount === 0, a.name)} />
          <span className="label">{a.name}</span>
          <span className="count">{visibleCount}/{all.length}</span>
        </div>
        {isOpen && (
          <ul>
            {childNodes}
            {shownParts.map((p) => (
              <li key={p.id} className={`row part ${selected === p.id ? 'selected' : ''}`} style={{ paddingLeft: 22 + depth * 12 }}>
                <input type="checkbox" checked={!hidden.has(p.id)} onChange={(e) => onToggleParts([p.id], e.target.checked, p.ref || p.name)} aria-label={`Show ${p.id}`} />
                <button className="part-name" onClick={() => onSelect(p, true)} title={p.name}>
                  {p.ref && <b>{p.ref}</b>} <span>{p.ref ? shortName(p) : p.name}</span>
                </button>
                {p.tbd.length > 0 && <span className="chip tbd" title={p.tbd.join('\n')}>TBD</span>}
              </li>
            ))}
          </ul>
        )}
      </li>
    );
  };

  return (
    <aside className="sidebar">
      <div className="sidebar-head">
        <input className="search" placeholder="Filter parts (ref, name, status)" value={query} onChange={(e) => setQuery(e.target.value)} />
        <div className="btn-row">
          <button onClick={onShowAll}>Show all</button>
          <button onClick={onHideAll}>Hide all</button>
          <button onClick={onIsolate} disabled={!selected} title="Show only the selected part">Isolate</button>
        </div>
      </div>
      <ul className="tree">{(tree.kids[''] || []).map((a) => node(a, 0))}</ul>
    </aside>
  );
}

function shortName(p) {
  const n = p.name.replace(/\s*\(.*?\)\s*/g, ' ').trim();
  return n.length > 38 ? `${n.slice(0, 36)}…` : n;
}
