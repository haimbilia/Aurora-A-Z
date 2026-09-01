GameListFilterCategories.AuroraAZ = {}

local function getFirstCharacter(Content)
    local name = Content.Name or ""
    return string.upper(string.sub(name, 1, 1))
end

local function makeLetterFilter(letter)
    return function(Content)
        return getFirstCharacter(Content) == letter
    end
end

GameListFilterCategories.AuroraAZ["A-Z #"] = function(Content)
    local first = getFirstCharacter(Content)
    return first == "" or string.match(first, "[A-Z]") == nil
end

for code = 65, 90 do
    local letter = string.char(code)
    GameListFilterCategories.AuroraAZ["A-Z " .. letter] = makeLetterFilter(letter)
end
