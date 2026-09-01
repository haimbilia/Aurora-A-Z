-- Aurora Alphabet Selector
-- Pure matching helpers kept separate from UI wiring so they are easy to test
-- and safe to reuse for either filtering or coverflow jump behavior.

local Alphabet = {}

Alphabet.Symbol = "#"
Alphabet.Letters = {
    "#", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
    "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"
}

local function firstCharacter(value)
    if type(value) ~= "string" or value == "" then
        return Alphabet.Symbol
    end

    return string.upper(string.sub(value, 1, 1))
end

function Alphabet.InitialForName(name)
    local initial = firstCharacter(name)
    if string.match(initial, "^[A-Z]$") then
        return initial
    end

    return Alphabet.Symbol
end

function Alphabet.MatchesName(name, letter)
    return Alphabet.InitialForName(name) == letter
end

-- Aurora filter predicate shape: function(Content) return boolean end
function Alphabet.CreateNameFilter(letter)
    return function(Content)
        return Content ~= nil and Alphabet.MatchesName(Content.Name, letter)
    end
end

return Alphabet

