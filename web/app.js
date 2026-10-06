// Ask the C++ server for transactions and show them in the table.

async function loadTransactions() {
  const status = document.getElementById("status");
  const rows = document.getElementById("rows");

  try {
    const response = await fetch("/api/transactions");
    if (!response.ok) throw new Error("server said " + response.status);
    const transactions = await response.json();

    rows.innerHTML = "";
    for (const t of transactions) {
      const tr = document.createElement("tr");
      const cells = [t.date, t.merchant, t.category, t.amount];
      cells.forEach((text, i) => {
        const td = document.createElement("td");
        td.textContent = text;              // textContent, never innerHTML, for data
        if (i === 3) td.className = "amount " + (t.cents < 0 ? "out" : "in");
        tr.appendChild(td);
      });
      rows.appendChild(tr);
    }
    status.textContent = transactions.length + " transactions, loaded from the C++ server.";
  } catch (err) {
    status.textContent = "Couldn't reach the server: " + err.message;
  }
}

loadTransactions();