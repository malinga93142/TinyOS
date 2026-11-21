-- Fenced code blocks -> plain lstlisting (no pandoc highlighting macros).
-- A language class (gas, c, sh) becomes language=...; \label replaces
-- pandoc's \hypertarget wrappers.
local langs = { gas = 'gas', c = 'C', sh = 'bash' }

function CodeBlock(el)
  local opt = ''
  local l = el.classes[1] and langs[el.classes[1]]
  if l then opt = '[language=' .. l .. ']' end
  return pandoc.RawBlock('latex',
    '\\begin{lstlisting}' .. opt .. '\n' .. el.text .. '\n\\end{lstlisting}')
end

function Header(el)
  local id = el.identifier
  el.identifier = ''
  if el.level == 1 and id ~= '' then
    return { el, pandoc.RawBlock('latex', '\\label{' .. id .. '}') }
  end
  return el
end
