package com.superessence.essabank.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import com.superessence.essabank.core.data.SqlDriver
import com.superessence.essabank.core.data.SqlRow

/**
 * Pilote SQLite Android pour [com.superessence.essabank.core.data.BankDatabase].
 * Le schéma est créé et migré par la couche commune ; ce helper ne fait qu'ouvrir le fichier.
 */
class AndroidSqlDriver(context: Context, name: String = DB_NAME) : SqlDriver {

    private val helper = object : SQLiteOpenHelper(context.applicationContext, name, null, 1) {
        override fun onConfigure(db: SQLiteDatabase) {
            db.enableWriteAheadLogging()
        }
        override fun onCreate(db: SQLiteDatabase) = Unit
        override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) = Unit
    }

    private val db: SQLiteDatabase get() = helper.writableDatabase

    override fun execute(sql: String, args: List<Any?>) {
        if (args.isEmpty()) db.execSQL(sql) else db.execSQL(sql, args.map { it }.toTypedArray())
    }

    override fun <T> query(sql: String, args: List<Any?>, map: (SqlRow) -> T): List<T> {
        db.rawQuery(sql, args.map { it?.toString() }.toTypedArray()).use { c ->
            val out = ArrayList<T>(c.count)
            val row = object : SqlRow {
                override fun string(index: Int): String? = if (c.isNull(index)) null else c.getString(index)
                override fun long(index: Int): Long? = if (c.isNull(index)) null else c.getLong(index)
            }
            while (c.moveToNext()) out += map(row)
            return out
        }
    }

    override fun <T> transaction(block: () -> T): T {
        val database = db
        database.beginTransaction()
        try {
            val result = block()
            database.setTransactionSuccessful()
            return result
        } finally {
            database.endTransaction()
        }
    }

    override fun close() = helper.close()

    companion object {
        const val DB_NAME = "essabank.db"
    }
}
