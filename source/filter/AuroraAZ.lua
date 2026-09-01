-- Aurora A-Z v0.1.0
-- Loaded by Aurora from User\Scripts\Content\Filters.

local CATEGORY_NAME = "A-Z"
local NON_LETTER = "#"

local function initialForName(name)
    if type(name) ~= "string" or name == "" then
        return NON_LETTER
    end

    local initial = string.upper(string.sub(name, 1, 1))
    if string.match(initial, "^[A-Z]$") == nil then
        return NON_LETTER
    end

    return initial
end

local function createFilter(initial)
    return function(Content)
        return Content ~= nil and initialForName(Content.Name) == initial
    end
end

local category = {}
GameListFilterCategories[CATEGORY_NAME] = category

category[NON_LETTER] = createFilter(NON_LETTER)

local groups = {
    { name = "A - F", letters = { "A", "B", "C", "D", "E", "F" } },
    { name = "G - L", letters = { "G", "H", "I", "J", "K", "L" } },
    { name = "M - R", letters = { "M", "N", "O", "P", "Q", "R" } },
    { name = "S - X", letters = { "S", "T", "U", "V", "W", "X" } },
    { name = "Y - Z", letters = { "Y", "Z" } }
}

for _, groupDefinition in ipairs(groups) do
    local group = {}
    category[groupDefinition.name] = group

    for _, letter in ipairs(groupDefinition.letters) do
        group[letter] = createFilter(letter)
    end
end
