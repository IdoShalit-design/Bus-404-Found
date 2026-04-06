function toggleInputs() {
  var mode = document.getElementById("state").value;
  var station = document.getElementById("stationOnly");
  var lines = document.getElementById("linesGroup");

  if (mode === "BUS_BY_LINES") {
    station.classList.add("hidden");
    lines.classList.remove("hidden");
    return;
  }

  if (mode === "USE_CURRENT_BUILD") {
    station.classList.add("hidden");
    lines.classList.add("hidden");
    return;
  }

  station.classList.remove("hidden");
  lines.classList.add("hidden");
}

document.addEventListener("DOMContentLoaded", function() {
  var mode = document.getElementById("state");
  mode.addEventListener("change", toggleInputs);
  toggleInputs();
});
