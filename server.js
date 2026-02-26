const express = require("express");
const mongoose = require("mongoose");
const cors = require("cors");

const app = express();
app.use(express.json());
app.use(cors());

// 🔹 Use Environment Variable for Security
const MONGO_URI = process.env.MONGO_URI;

mongoose.connect(MONGO_URI)
.then(() => console.log("✅ MongoDB Atlas Connected"))
.catch(err => console.log(err));

// Schema
const logSchema = new mongoose.Schema({
    status: String,
    name: String,
    id: Number,
    timestamp: String
});

const Log = mongoose.model("Log", logSchema);

// POST Route
app.post("/log", async (req, res) => {
    try {
        const newLog = new Log(req.body);
        await newLog.save();
        res.status(200).send("Saved to Atlas");
    } catch (err) {
        res.status(500).send(err.message);
    }
});

app.get("/", (req, res) => {
    res.send("Biometric Cloud API Running 🚀");
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
    console.log("Server running on port " + PORT);
});