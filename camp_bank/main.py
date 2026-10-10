from fastapi import FastAPI, Depends, HTTPException
from sqlalchemy.orm import Session
import models, database, schemas
from fastapi.responses import FileResponse


app = FastAPI(title="CampBank IoT System")

# Створюємо базу даних при запуску
models.Base.metadata.create_all(bind=database.engine)

@app.get("/")
def read_root():
    return {"status": "CampBank Server is running"}

# 1. Отримати баланс по UID карти
@app.get("/balance/{uid}")
def get_balance(uid: str, db: Session = Depends(database.get_db)):
    user = db.query(models.User).filter(models.User.nfc_uid == uid).first()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    return {"name": user.name, "balance": user.balance}

# 2. Провести оплату (зняття коштів)
@app.post("/transaction")
def make_transaction(uid: str, amount: float, term_id: str, db: Session = Depends(database.get_db)):
    if amount <= 0:
        raise HTTPException(status_code=400, detail="Amount must be greater than zero")
        
    user = db.query(models.User).filter(models.User.nfc_uid == uid).first()
    
    if not user:
        raise HTTPException(status_code=404, detail="Card not registered")
    
    if user.balance < amount:
        raise HTTPException(status_code=400, detail="Insufficient funds")
    
    # Оновлюємо баланс
    user.balance -= amount
    
    # Записуємо історію
    new_tx = models.Transaction(user_id=user.id, amount=-amount, terminal_id=term_id)
    db.add(new_tx)
    db.commit()
    
    return {"status": "success", "new_balance": user.balance, "name": user.name}

# 3. Реєстрація нового користувача (для адміна)
@app.post("/users")
def create_user(name: str, uid: str, initial_balance: float, db: Session = Depends(database.get_db)):
    # Перевіряємо, чи немає вже такої картки
    existing_user = db.query(models.User).filter(models.User.nfc_uid == uid).first()
    if existing_user:
        raise HTTPException(status_code=400, detail="Card already registered to another user")

    db_user = models.User(name=name, nfc_uid=uid, balance=initial_balance)
    db.add(db_user)
    db.commit()
    return {"status": "User created", "uid": uid}

# 4. Переказ коштів між дітьми (P2P)
@app.post("/transfer")
def transfer_money(from_uid: str, to_uid: str, amount: float, db: Session = Depends(database.get_db)):
    if amount <= 0:
        raise HTTPException(status_code=400, detail="Amount must be greater than zero")
        
    sender = db.query(models.User).filter(models.User.nfc_uid == from_uid).first()
    receiver = db.query(models.User).filter(models.User.nfc_uid == to_uid).first()
    
    if not sender or not receiver:
        raise HTTPException(status_code=404, detail="One or both cards not found")
    
    if sender.balance < amount:
        raise HTTPException(status_code=400, detail="Insufficient funds")
    
    # Виконуємо переказ
    sender.balance -= amount
    receiver.balance += amount
    
    # Записуємо історію для обох
    tx_out = models.Transaction(user_id=sender.id, amount=-amount, terminal_id="P2P_Transfer")
    tx_in = models.Transaction(user_id=receiver.id, amount=amount, terminal_id="P2P_Transfer")
    db.add(tx_out)
    db.add(tx_in)
    db.commit()
    
    return {"status": "success"}

# --- АДМІН ПАНЕЛЬ ТА КОМАНДИ ---

# Відображення сторінки адмін-панелі
@app.get("/admin", response_class=FileResponse)
def get_admin_panel():
    return "admin.html"

# Отримати список всіх користувачів
@app.get("/users/all")
def get_all_users(db: Session = Depends(database.get_db)):
    users = db.query(models.User).all()
    return users

# Змінити команду користувача
@app.put("/users/{uid}/team")
def assign_team(uid: str, team: str, db: Session = Depends(database.get_db)):
    user = db.query(models.User).filter(models.User.nfc_uid == uid).first()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    user.team = team
    db.commit()
    return {"status": "success", "new_team": user.team}

# Нарахувати/списати кошти конкретному користувачу
@app.post("/admin/transaction/user")
def admin_user_transaction(uid: str, amount: float, db: Session = Depends(database.get_db)):
    user = db.query(models.User).filter(models.User.nfc_uid == uid).first()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    
    user.balance += amount
    new_tx = models.Transaction(user_id=user.id, amount=amount, terminal_id="admin_panel")
    db.add(new_tx)
    db.commit()
    return {"status": "success", "new_balance": user.balance}

# Нарахувати/списати кошти всій команді
@app.post("/admin/transaction/team")
def admin_team_transaction(team: str, amount: float, db: Session = Depends(database.get_db)):
    users = db.query(models.User).filter(models.User.team == team).all()
    if not users:
        raise HTTPException(status_code=404, detail="Team not found or empty")
    
    for u in users:
        u.balance += amount
        new_tx = models.Transaction(user_id=u.id, amount=amount, terminal_id="admin_panel")
        db.add(new_tx)
        
    db.commit()
    return {"status": "success", "updated_count": len(users)}