package org.json;
import java.util.ArrayList;
/** Desktop shim (tools/companion). */
public class JSONArray {
    final ArrayList<Object> l = new ArrayList<>();
    public int length() { return l.size(); }
    public Object opt(int i) { return i < l.size() ? l.get(i) : null; }
    public String optString(int i, String d) { Object o = opt(i); return o instanceof String ? (String) o : d; }
    public int optInt(int i, int d) { Object o = opt(i); return o instanceof Number ? ((Number) o).intValue() : d; }
    public double optDouble(int i, double d) { Object o = opt(i); return o instanceof Number ? ((Number) o).doubleValue() : d; }
    public JSONArray optJSONArray(int i) { Object o = opt(i); return o instanceof JSONArray ? (JSONArray) o : null; }
}
