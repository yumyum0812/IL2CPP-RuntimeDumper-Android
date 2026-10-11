import * as ksu2 from "./ext/kernelsu-alt.js";

const message = document.getElementById("message");
const list = document.getElementById("packageList");
const input = document.getElementById("packageName");
const form = document.getElementById("addForm");

let packages = new Set();
const filePath = "/data/adb/runtimedumper-packages.txt";
const testMode = !ksu2.isKsuWebui()

const quote = value => "'" + String(value).replace(/'/g, "'\\''") + "'";

async function runcommand(command) {
    const res = await ksu2.exec(command);

    if (res.errno !== 0) {
        throw new Error(
            "err code " + res.errno + ": " + res.stderr
        );
    }

    return res.stdout;
}

async function loadPackages() {
    let raw = ""

    const a = await ksu2.exec("[ -f " + quote(filePath) + " ]")
    if (a.errno == 0) {
        raw = await runcommand(
            "cat " + quote(filePath)
        );
    }

    console.log(raw)

    const data = raw.split("\n").filter(s => isValidPackage(s.trim()))
    packages = new Set(data);
    render();
}

async function savePackages() {
    const s = [...packages].join("\n");

    try {
        await runcommand("printf '%s' " + quote(s) + " > " + quote(filePath));
    } catch (e) {
        throw new Error("Failed to save config: " + e);
    }

    render();
}

function isValidPackage(name) {
    return /^[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)+$/.test(name)
}

function removePackage(name) {
    packages = new Set([...packages].filter(x => x !== name))
}

async function render() {
    list.replaceChildren();

    if (packages.length === 0) {
        const p = document.createElement("p");
        p.textContent = "No packages registered";
        list.appendChild(p);
        return;
    }

    packages.forEach(name => {
        const row = document.createElement("tr");

        const label = document.createElement("td");
        label.textContent = name;
        label.style.cssText = `
            max-width: 0;
            width: 100%;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;`;

        const btnTd = document.createElement("td");
        btnTd.style.cssText = "text-align: right; white-space: nowrap;";

        const button = document.createElement("button");
        button.type = "button";
        button.textContent = "Remove";
        button.style.cssText = "padding: 4px 10px; font-size: 0.9rem; margin: 0;";

        row.append(label, btnTd);
        btnTd.append(button)
        list.appendChild(row);

        button.onclick = async function () {
            try {
                message.textContent = "Saving...";

                removePackage(name);
                render();

                if (!testMode) {
                    await savePackages();
                }

                message.textContent = "Removed: " + name;
            } catch (e) {
                message.textContent = "Remove error: " + String(e);
                console.error("remove error:", e);
            }
        };
    });
}

function addPackage(name) {
    packages.add(name)
}

async function main() {
    if (!testMode) {
        try {
            await loadPackages();
            message.textContent = "Loaded " + packages.size + " packages";
        } catch (e) {
            message.textContent = "Initialization error: " + String(e);
        }
    } else {
        packages = new Set(["com.example.app", "com.hello.world", "com.yjsn.pi"])
        render()
        message.textContent = "Loaded " + packages.size + " packages";
    }

    form.addEventListener("submit", async event => {
        event.preventDefault();

        const name = input.value.trim();

        if (!isValidPackage(name)) {
            message.textContent = "Invalid package format";
            return;
        }

        try {
            if (packages.has(name)) {
                message.textContent = "The pacakge already registered";
                return;
            }

            addPackage(name)

            if (!testMode) {
                await savePackages();
            }

            input.value = "";
            message.textContent = "Registered: " + name;
            render()
        } catch (e) {
            message.textContent = "Failed to register: " + String(e);
        }
    });
}

main()