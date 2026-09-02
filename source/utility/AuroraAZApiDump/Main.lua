scriptTitle = "Aurora A-Z API Dump"
scriptAuthor = "haimbilia"
scriptVersion = 1
scriptDescription = "Read-only. Dumps the Lua API surface to the Aurora debug log."
scriptPermissions = { "sql" }

-- This script writes nothing and changes nothing. Everything it learns goes to
-- Data\Logs\debug.log via print(), which Aurora records as "LUA > <text>".

local function say(line)
    print("APIDUMP " .. line)
end

local function dumpTable(name, value)
    if type(value) ~= "table" then
        say("[" .. name .. "] is " .. type(value))
        return
    end

    local keys = {}
    for k, v in pairs(value) do
        keys[#keys + 1] = tostring(k) .. "  <" .. type(v) .. ">"
    end
    table.sort(keys)

    say("[" .. name .. "] " .. #keys .. " entries")
    for i = 1, #keys do
        say("   " .. name .. "." .. keys[i])
    end
end

local function probeSql(label, query)
    local ok, rows = pcall(Sql.ExecuteFetchRows, query)
    if not ok then
        say("SQL " .. label .. " -> threw: " .. tostring(rows))
        return
    end
    if rows == nil then
        say("SQL " .. label .. " -> nil (query rejected or wrong database)")
        return
    end
    say("SQL " .. label .. " -> " .. #rows .. " row(s)")
    if rows[1] ~= nil then
        for k, v in pairs(rows[1]) do
            say("   row1." .. tostring(k) .. " = " .. tostring(v))
        end
    end
end

function main()
    say("===== BEGIN =====")
    say("_VERSION = " .. tostring(_VERSION))

    dumpTable("_G", _G)

    -- Packages the loader reports at startup, plus plausible extras.
    local names = {
        "Script", "Aurora", "Content", "ContentItem", "Sql", "Profile",
        "FileSystem", "Http", "IniFile", "ZipFile", "Thread", "Dvd", "Kernel",
        "Settings", "GameList", "GameListManager", "QuickView", "QuickViews",
        "Xui", "XuiScene", "Menu", "Filesystem", "Sound", "Video"
    }
    for i = 1, #names do
        local n = names[i]
        if _G[n] ~= nil then
            dumpTable(n, _G[n])
        end
    end

    -- Enums loaded by User\Scripts\Main.lua, needed to write correct filters.
    local enums = {
        "ContentFlag", "SortType", "XuiMessage", "XuiObject", "PermissionFlag",
        "KeyboardFlag", "PopupType", "GizmoCommand", "FileAttribute", "ScriptMode"
    }
    for i = 1, #enums do
        local n = enums[i]
        if _G[n] ~= nil then
            dumpTable(n, _G[n])
        end
    end

    -- Which database does the Sql package actually talk to?
    probeSql("settings/QuickViews", "SELECT COUNT(*) AS N FROM QuickViews")
    probeSql("content/ContentItems", "SELECT COUNT(*) AS N FROM ContentItems")
    probeSql("content/sample", "SELECT Id, TitleName FROM ContentItems ORDER BY TitleName LIMIT 1")

    say("===== END =====")

    Script.ShowMessageBox(
        "Aurora A-Z API Dump",
        "Done. The API listing was written to Data\\Logs\\debug.log.\n\n" ..
        "Nothing was modified.",
        "OK"
    )
end
