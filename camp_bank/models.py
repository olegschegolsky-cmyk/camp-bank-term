from sqlalchemy import Column, Integer, String, Float, DateTime, ForeignKey, Boolean
from sqlalchemy.orm import relationship
from datetime import datetime
from database import Base

class User(Base):
    __tablename__ = "users"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String)
    nfc_uid = Column(String, unique=True, index=True)
    balance = Column(Float, default=0.0)
    team = Column(String, default="Без команди") # Нове поле
    is_blocked = Column(Boolean, default=False)  # Нове поле
    
    transactions = relationship("Transaction", back_populates="owner")

class Transaction(Base):
    __tablename__ = "transactions"

    id = Column(Integer, primary_key=True, index=True)
    user_id = Column(Integer, ForeignKey("users.id"))
    amount = Column(Float)
    timestamp = Column(DateTime, default=datetime.utcnow)
    terminal_id = Column(String)

    owner = relationship("User", back_populates="transactions")