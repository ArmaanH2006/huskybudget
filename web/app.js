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
  drawCategoryChart(s.spendingByCategory, s.spending.cents);
}

// One horizontal bar per category, longest first. The server already sorted
// them biggest-first. Bar length is relative to the biggest category; the
// percentage is each category's share of all spending.
function drawCategoryChart(categories, totalCents) {
  const chart = document.getElementById("category-chart");
  const empty = document.getElementById("chart-empty");
  chart.innerHTML = "";
  empty.hidden = categories.length > 0;
  if (categories.length === 0) return;

  const biggest = categories[0].amount.cents;
  for (const c of categories) {
    const share = Math.round((c.amount.cents / totalCents) * 100);

    const row = document.createElement("div");
    row.className = "bar-row";
    row.setAttribute("role", "row");
    row.title = c.category + ": " + c.amount.display + " (" + share + "% of spending)";

    const label = document.createElement("span");
    label.className = "bar-label";
    label.textContent = c.category;

    const track = document.createElement("div");
    track.className = "bar-track";
    const fill = document.createElement("div");
    fill.className = "bar-fill";
    fill.style.width = (c.amount.cents / biggest) * 100 + "%";
    track.appendChild(fill);

    const value = document.createElement("span");
    value.className = "bar-value";
    value.textContent = c.amount.display;
    const pct = document.createElement("span");
    pct.className = "bar-share";
    pct.textContent = share + "%";
    value.appendChild(pct);

    row.append(label, track, value);
    chart.appendChild(row);
  }
}

// One row per budget: name, "spent of limit", a progress bar, and a message.
// The server already did the math and sorted them most-used first.
async function loadBudgets(month) {
  const budgets = await getJson("/api/budgets?month=" + month);
  const list = document.getElementById("budgets");
  document.getElementById("budgets-empty").hidden = budgets.length > 0;
  list.innerHTML = "";

  for (const b of budgets) {
    const row = document.createElement("div");
    row.className = "budget " + b.health;          // "on-track", "at-risk", or "over"

    const top = document.createElement("div");
    top.className = "budget-top";
    const name = document.createElement("span");
    name.className = "budget-name";
    name.textContent = b.category;
    const badge = document.createElement("span");
    badge.className = "badge";
    badge.textContent = { "on-track": "On track", "at-risk": "At risk", "over": "Over" }[b.health];
    name.appendChild(badge);
    const amounts = document.createElement("span");
    amounts.className = "budget-amounts";
    amounts.textContent = b.spent.display + " of " + b.limit.display + " · " + b.percentUsed + "%";
    top.append(name, amounts);

    const track = document.createElement("div");
    track.className = "budget-track";
    const fill = document.createElement("div");
    fill.className = "budget-fill";
    fill.style.width = Math.min(b.percentUsed, 100) + "%";   // a bar can't go past full
    track.appendChild(fill);

    const note = document.createElement("p");
    note.className = "budget-note";
    note.textContent = budgetMessage(b);

    row.append(top, track, note);
    list.appendChild(row);
  }
}

function budgetMessage(b) {
  if (b.health === "over") {
    return "Over by " + b.remaining.display.replace("-", "");
  }
  if (b.health === "at-risk") {
    return "At this pace, about " + b.projected.display + " by the end of the month";
  }
  let message = b.remaining.display + " left";
  if (b.tooEarly) message += " · too early in the month to judge the pace";
  return message;
}

// ---- spending score --------------------------------------------------------

async function loadScore(month) {
  const s = await getJson("/api/score?month=" + month);
  document.getElementById("score-total").textContent = s.total;
  const grade = document.getElementById("score-grade");
  grade.textContent = s.grade;
  grade.className = "score-grade grade-" + s.grade;

  // One row per part: name, points, a small bar, and how it was earned.
  const parts = document.getElementById("score-parts");
  parts.innerHTML = "";
  for (const p of s.parts) {
    const row = document.createElement("div");
    row.className = "score-part";

    const top = document.createElement("div");
    top.className = "score-part-top";
    const name = document.createElement("span");
    name.textContent = p.name;
    const points = document.createElement("span");
    points.className = "score-points";
    points.textContent = p.points + " / " + p.maxPoints;
    top.append(name, points);

    const track = document.createElement("div");
    track.className = "score-track";
    const fill = document.createElement("div");
    fill.className = "score-fill";
    fill.style.width = (p.points / p.maxPoints) * 100 + "%";
    track.appendChild(fill);

    const detail = document.createElement("p");
    detail.className = "score-detail";
    detail.textContent = p.detail;

    row.append(top, track, detail);
    parts.appendChild(row);
  }

  const tips = document.getElementById("tips");
  tips.innerHTML = "";
  for (const tip of s.tips) {
    const item = document.createElement("li");
    item.textContent = tip;
    tips.appendChild(item);
  }
}

// ---- alerts ----------------------------------------------------------------

const alertTitles = {
  "unusual-amount": "Unusually large",
  "duplicate":      "Possible double charge",
  "new-merchant":   "Big buy, new store",
  "burst":          "Spending burst",
};

// Ids of the transactions that have an alert, so their rows can be marked.
let flaggedIds = new Set();

async function loadAlerts(month) {
  const alerts = await getJson("/api/alerts?month=" + month);
  const list = document.getElementById("alerts");
  document.getElementById("alerts-empty").hidden = alerts.length > 0;
  list.innerHTML = "";
  flaggedIds = new Set(alerts.map(a => a.transactionId).filter(id => id !== 0));

  for (const a of alerts) {
    const item = document.createElement("li");
    item.className = "alert " + a.kind;

    const top = document.createElement("div");
    top.className = "alert-top";
    const title = document.createElement("span");
    title.className = "alert-title";
    title.textContent = alertTitles[a.kind];
    const when = document.createElement("span");
    when.className = "alert-when";
    when.textContent = a.date + (a.merchant ? " · " + a.merchant : "");
    title.appendChild(when);
    const amount = document.createElement("span");
    amount.className = "alert-amount";
    amount.textContent = a.amount.display;
    top.append(title, amount);

    const reason = document.createElement("p");
    reason.className = "alert-reason";
    reason.textContent = a.reason;              // the server explains why, in plain English

    item.append(top, reason);
    list.appendChild(item);
  }
}

// Puts a warning mark on table rows that have an alert.
function markFlaggedRows() {
  for (const tr of document.getElementById("rows").children) {
    tr.classList.toggle("flagged", flaggedIds.has(Number(tr.dataset.id)));
  }
}

async function loadTransactions(month) {
  const status = document.getElementById("status");
  const rows   = document.getElementById("rows");
  const transactions = await getJson("/api/transactions?month=" + month);

  rows.innerHTML = "";
  for (const t of transactions) {
    const tr = document.createElement("tr");
    tr.dataset.id = t.id;                    // so alerts can find this row

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
    await Promise.all([loadSummary(month), loadScore(month), loadBudgets(month),
                       loadAlerts(month), loadTransactions(month)]);
    markFlaggedRows();                       // needs both the alerts and the table
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

// ---- setting a budget ------------------------------------------------------

const budgetForm  = document.getElementById("budget-form");
const budgetError = document.getElementById("budget-error");

budgetForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  budgetError.hidden = true;

  const data = Object.fromEntries(new FormData(budgetForm));
  const response = await fetch("/api/budgets", {
    method: "PUT",                           // PUT: add it, or replace the limit if it exists
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(data),
  });

  if (!response.ok) {
    const body = await response.json();
    budgetError.textContent = body.error;
    budgetError.hidden = false;
    return;
  }
  budgetForm.reset();
  refresh();
});

// ---- what if I saved instead? ----------------------------------------------

const whatifForm  = document.getElementById("whatif-form");
const whatifError = document.getElementById("whatif-error");

async function calculateWhatIf() {
  whatifError.hidden = true;
  const query = new URLSearchParams(new FormData(whatifForm));   // monthly=200&rate=7&years=10
  const response = await fetch("/api/whatif?" + query);
  const r = await response.json();
  if (!response.ok) {
    whatifError.textContent = r.error;
    whatifError.hidden = false;
    return;
  }

  document.getElementById("whatif-answer").textContent =
    "You'd have " + r.finalBalance.display + " after " + r.years + " years: " +
    r.contributed.display + " you put in, plus " + r.growth.display + " of growth.";

  // One bar per year, so you can see it speed up over time.
  const chart = document.getElementById("whatif-chart");
  chart.innerHTML = "";
  const biggest = r.finalBalance.cents;
  r.balanceByYear.forEach((balance, i) => {
    const row = document.createElement("div");
    row.className = "bar-row";
    const label = document.createElement("span");
    label.className = "bar-label";
    label.textContent = "Year " + (i + 1);
    const track = document.createElement("div");
    track.className = "bar-track";
    const fill = document.createElement("div");
    fill.className = "bar-fill savings";
    fill.style.width = (balance.cents / biggest) * 100 + "%";
    track.appendChild(fill);
    const value = document.createElement("span");
    value.className = "bar-value";
    value.textContent = balance.display;
    row.append(label, track, value);
    chart.appendChild(row);
  });
}

whatifForm.addEventListener("submit", (event) => {
  event.preventDefault();
  calculateWhatIf();
});

// ---- importing a CSV -------------------------------------------------------

const importForm   = document.getElementById("import-form");
const importResult = document.getElementById("import-result");

importForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const file = importForm.file.files[0];
  if (!file) return;

  const response = await fetch("/api/import", {
    method: "POST",
    headers: { "Content-Type": "text/csv" },
    body: await file.text(),                 // send the file's contents as-is
  });
  const body = await response.json();

  importResult.hidden = false;
  importResult.innerHTML = "";
  if (response.ok) {
    importResult.className = "success";
    importResult.textContent = "Imported " + body.imported + " transactions from " + file.name + ".";
    importForm.reset();
    refresh();
    return;
  }

  // Show what went wrong, line by line (first 10 problems).
  importResult.className = "error";
  importResult.textContent = "Nothing was imported. Fix these and try again:";
  const list = document.createElement("ul");
  for (const p of (body.problems || []).slice(0, 10)) {
    const item = document.createElement("li");
    item.textContent = "Line " + p.line + ": " + p.message;
    list.appendChild(item);
  }
  importResult.appendChild(list);
});

// ---- start -----------------------------------------------------------------

monthInput.value = today().slice(0, 7);
form.date.value = today();
monthInput.addEventListener("change", refresh);
refresh();
calculateWhatIf();                           // show the example right away