package org.json;

/** Like Android's: a checked exception, so code that compiles here also compiles against android.jar. */
public class JSONException extends Exception {
    public JSONException(String message) { super(message); }
}
