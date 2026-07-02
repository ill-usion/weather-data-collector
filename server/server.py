import time
import json
import pandas as pd
import sqlite3
from flask import Flask, request,g 

app = Flask(__name__)
DB_PATH = "./weather.db"


def get_db():
    db = getattr(g, "_database", None)
    if db is None:
        db = g._database = sqlite3.connect(DB_PATH)
        db.execute("""CREATE TABLE IF NOT EXISTS weather (
    timestamp INTEGER PRIMARY KEY,
    temp1     REAL,
    temp2     REAL,
    pressure  REAL,
    humidity  REAL,
    heat_idx  REAL,
    battery   REAL
)""")
        db.commit()
    
    return db


@app.teardown_appcontext
def close_db_conn(exception):
    db = getattr(g, "_database", None)
    if db is not None:
        db.close()


@app.get("/test")
def test():
    return "Hello, World!", 200


@app.get("/timestamp")
def timestamp():
    return str(int(time.time())), 200


@app.post("/batch-submit")
def batch_submit():
    with app.app_context():
        db = get_db()
        cur = db.cursor()
        data = request.get_json(True)
        print(data)
        entries = data.get("entries", None)
        try:
            if entries is None:
                raise Exception("entries are missing")

            tupled = [(e["timestamp"], e["temp1"], e["temp2"], e["pressure"], e["humidity"], e["heat_index"], e["battery"]) for e in entries]
            cur.executemany("INSERT INTO weather VALUES(?, ?, ?, ?, ?, ?, ?)", tupled)
        except Exception as e:
            # Assuming that the above code fails because of a bad input
            return f"Bad data: {e}", 400

        db.commit()
        return "", 204


@app.get("/get-latest")
def get_latest():
    n = request.args.get("n", default=5, type=int)
    with app.app_context():
        db = get_db()
        cur = db.cursor()

        response = [{
            "timestamp": row[0],
            "temp1": row[1],
            "temp2": row[2],
            "pressure": row[3],
            "humidity": row[4],
            "heat_index": row[5],
            "battery": row[6]
        } for row in cur.execute("SELECT * FROM weather ORDER BY timestamp DESC LIMIT ?", (n, ))]

        return response, 200



if __name__ == "__main__":
    app.run("0.0.0.0", 4567, debug=True)