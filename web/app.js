// HuskyBudget frontend. Talks to the C++ server's JSON API.

const monthInput = document.getElementById("month");
const form       = document.getElementById("add-form");
const formError  = document.getElementById("form-error");

// ---- helpers ---------------------------------------------------------------

// Today as "2026-10-07", in the user's own timezone.
// (toISOString() would use UTC, which is already "tomorrow" on a US evening.)
function today() {
  const now = new Date();
  const two = (n) => String(n).padStart(2, "0");
  return now.getFullYear() + "-" + two(now.getMonth() + 1) + "-" + two(now.getDate());
}

async function getJson(url) {
  const response = await fetch(url);
  const body = await response.json();
  if (!response.ok) throw new Error(body.error || "server said " + response.status);
  return body;
}

function setMoney(elementId, money) {
  const el = document.getElementById(elementId);
  el.textContent = money.display;
  if (elementId === "net") el.className = "value " + (money.cents < 0 ? "out" : "in");
}

// ---- loading data ----------------------------------------------------------

async function loadSummary(month) {
  const s = await getJson("/api/summary?month=" + month);
  setMoney("income", s.income);
  setMoney("spending", s.spending);
  setMoney("net", s.net);
}

async function loadTransactions(month) {
  const status = document.getElementById("status");
  const rows   = document.getElementById("rows");
  const transactions = await getJson("/api/transactions?month=" + month);

  rows.innerHTML = "";
  for (const t of transactions) {
    const tr = document.createElement("tr");

    for (const text of [t.date, t.merchant, t.category]) {
      const td = document.createElement("td");
      td.textContent = text;                 // textContent, never innerHTML, for data
      tr.appendChild(td);
    }

    const amount = document.createElement("td");
    amount.textContent = t.amount;
    amount.className = "num " + (t.cents < 0 ? "out" : "in");
    tr.appendChild(amount);

    const actions = document.createElement("td");
    const del = document.createElement("button");
    del.className = "delete";
    del.title = "Delete";
    del.textContent = "×";
    del.addEventListener("click", () => deleteTransaction(t));
    actions.appendChild(del);
    tr.appendChild(actions);

    rows.appendChild(tr);
  }

  status.textContent = transactions.length === 0
    ? "No transactions this month."
    : transactions.length + " transactions";

  // Suggest categories the user has already used.
  const list = document.getElementById("categories");
  list.innerHTML = "";
  for (const c of new Set(transactions.map(t => t.category))) {
    const option = document.createElement("option");
    option.value = c;
    list.appendChild(option);
  }
}

async function refresh() {
  const month = monthInput.value;
  try {
    await Promise.all([loadSummary(month), loadTransactions(month)]);
  } catch (err) {
    document.getElementById("status").textContent = "Couldn't reach the server: " + err.message;
  }
}

// ---- changing data ---------------------------------------------------------

form.addEventListener("submit", async (event) => {
  event.preventDefault();                    // stop the browser reloading the page
  formError.hidden = true;

  const data = Object.fromEntries(new FormData(form));
  const response = await fetch("/api/transactions", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(data),
  });

  if (!response.ok) {
    const body = await response.json();
    formError.textContent = body.error;      // the server's validation message
    formError.hidden = false;
    return;
  }

  // Jump to the month of the new transaction so the user sees it appear.
  monthInput.value = data.date.slice(0, 7);
  form.reset();
  form.date.value = data.date;               // keep the date, it's usually the same
  refresh();
});

async function deleteTransaction(t) {
  if (!confirm("Delete " + t.amount + " at " + (t.merchant || t.category) + "?")) return;
  const response = await fetch("/api/transactions/" + t.id, { method: "DELETE" });
  if (!response.ok) alert("Couldn't delete that transaction.");
  refresh();
}

// ---- start -----------------------------------------------------------------

monthInput.value = today().slice(0, 7);
form.date.value = today();
monthInput.addEventListener("change", refresh);
refresh();