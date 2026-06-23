'''
 * ============================================================================
 * @file        data_base.py
 *
 * @author      Marco Antônio Ranghetti
 * @github      github.com/mRangh
 * @email       marcoantonioranghetti@gmail.com
 * @academic    d2026008956@unifei.edu.br
 *
 * @version     1.0.0
 * @date        2026-06-22
 * @license     Apache License 2.0
 * ============================================================================
 '''

import os
os.environ["QT_QPA_PLATFORM"] = "xcb"
os.environ["QT_LOGGING_RULES"] = "*.warning=false"
import sqlite3
import vision_drivers as vd
from datetime import datetime, timedelta



DB_NAME = 'parking.db'



def init_db():
    with sqlite3.connect(DB_NAME) as conn:
        cursor = conn.cursor()

        cursor.execute('''
            CREATE TABLE IF NOT EXISTS vehicles (
                hash TEXT PRIMARY KEY,
                payed INTEGER DEFAULT 0,
                entry_time TEXT,
                expiry_time TEXT
            )
        ''')

        print('[SQLITE]: Database initialized.')

init_db()



def register_entry(hash: str) -> bool:
    act_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    try:
        with sqlite3.connect(DB_NAME) as conn:
            cursor = conn.cursor()
            cursor.execute(
                "INSERT OR REPLACE INTO vehicles (hash, payed, entry_time, expiry_time) VALUES (?, 0, ?, NULL)",
                (hash, act_time)
            )
        print(f'[SQLITE]: Plate {hash} registered at {act_time}.')
        return True
    
    except Exception as e:
        print(f'[SQLITE_ERR]: Could not register hash {hash} due to {e}.')
        return False



def delete_entry(hash: str) -> bool:

    try:
        with sqlite3.connect(DB_NAME) as conn:
            cursor = conn.cursor()
            cursor.execute('DELETE FROM vehicles WHERE hash = ?', (hash,))
            conn.commit()
            
            # rowcount diz quantas linhas foram afetadas pelo DELETE
            if cursor.rowcount > 0:
                print(f"[SQLITE]: Hash {hash[:10]}... deleted successfully.")
                return True
            else:
                print(f"[SQLITE_WARN]: No record found for Hash {hash[:10]}... to delete.")
                return False
                
    except sqlite3.Error as e:
        print(f"[SQLITE_ERR]: Failed to delete record: {e}")
        return False



def pay_parking(hash: str) -> bool:
    try:
        with sqlite3.connect(DB_NAME) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT entry_time FROM vehicles WHERE hash = ?", (hash,))
            result = cursor.fetchone()

            if not result:
                print(f'[SQLITE_WARN]: Plate {hash} not found to process payment.')
                return False
            
            entry_time_str = result[0]
            entry_time = datetime.strptime(entry_time_str, '%Y-%m-%d %H:%M:%S')
            pay_time = datetime.now()

            total_h = (pay_time - entry_time).total_seconds() // 3600
            next_full_h = entry_time + timedelta(hours=total_h + 1)
            minimal_tolerance = pay_time + timedelta(minutes=20)

            expiry_time = max(next_full_h, minimal_tolerance)
            expiry_time_str = expiry_time.strftime('%Y-%m-%d %H:%M:%S')

            cursor.execute(
                "UPDATE vehicles SET payed = 1, expiry_time = ? WHERE hash = ?",
                (expiry_time_str, hash)
            )
            has_updated = cursor.rowcount > 0

        if has_updated:
            print(f'[SQLITE]: Plate {hash} paid. Exit allowed until {expiry_time_str}.')
            return True
        else:
            print(f'[SQLITE_WARN]: Plate {hash} not found.')
            return False
        
    except Exception as e:
        print(f'[SQLITE_ERR]: Could not pay plate {hash} due to {e}.')
        return False



def verify_payment(hash: str) -> bool:
    try:
        with sqlite3.connect(DB_NAME) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT payed, expiry_time, entry_time FROM vehicles WHERE hash = ?", (hash,))
            result = cursor.fetchone()

        if result is not None:
            payed, expiry_time_str, entry_time_str = result
        
            entry_time = datetime.strptime(entry_time_str, '%Y-%m-%d %H:%M:%S')
            now = datetime.now()
            time_spent = now - entry_time
            
            if time_spent <= timedelta(minutes=10):
                print(f'[SQLITE]: Plate {hash} authorized via Initial Courtesy! Time spent: {time_spent}.')
                return True

            if payed == 0:
                print(f'[SQLITE_WARN]: Plate {hash} not paid.')
                return False

            expiry_time = datetime.strptime(expiry_time_str, '%Y-%m-%d %H:%M:%S')

            if now <= expiry_time:
                print(f'[SQLITE]: Plate {hash} authorized. Exit allowed until {expiry_time_str}')
                return True
            else:
                print(f'[SQLITE_WARN]: Plate {hash} out of exit window, expired at {expiry_time_str}.')
                with sqlite3.connect(DB_NAME) as conn:
                    cursor = conn.cursor()
                    cursor.execute("UPDATE vehicles SET payed = 0 WHERE hash = ?", (hash,))
                return False
        print(f'[SQLITE_WARN]: Plate {hash} not found.')
        return False

    except Exception as e:
        print(f'[SQLITE_ERR]: Could not verify payment for plate {hash} due to {e}.')
        return False



if __name__ == '__main__':
    init_db()
    hash = vd.hash_sha256(vd.generate_random_plate())
    register_entry(hash)
    vd.generate_qr_code(hash)
    pay_parking(hash)

