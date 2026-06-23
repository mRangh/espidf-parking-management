'''
 * ============================================================================
 * @file        callbacks.py
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

import data_base as db
import vision_drivers as vd
import time



def entry_callback() -> bool:

    print('[CALLBACK]: Vehicle entry detected.')
    plate = vd.hash_sha256(vd.generate_random_plate())
    qr_code = vd.generate_qr_code(plate)
    # termical_printer.print(qr_code)
    registered = db.register_entry(plate)
    if registered:
        return True
    return False



def exit_callback() -> bool:

    print('[CALLBACK]: Vehicle exit detected.')
    plate = input('[WORKBENCH_TEST]: Manual input:')
    print('[PYTHON]: Starting OPENCV test in 3 seconds')
    for i in range(3, 0, -1):
        print(f"{i}...")
        time.sleep(1)
    for _ in range(10):
        # plate = vd.hash_sha256(vd.read_license_plate())
        qr_code_read = vd.read_qr_code()
        if plate == qr_code_read:
            print('[PYTHON]: Both hashes matched. Verifying payment...')
            if db.verify_payment(plate):
                db.delete_entry(plate)
                print('[PYTHON]: Payment good to go. Allowing exit.')
                return True
            else:
                print('[PYTHON]: Parking not yet payed. Blocking exit.')
                return False
    print('[PYTHON]: Strings do not match. Blocking exit.')
    return False