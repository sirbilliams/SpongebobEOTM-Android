package org.json;
import java.util.HashMap;
/** Desktop shim (tools/companion): the opt* calls CompanionView makes. */
public class JSONObject {
    final HashMap<String, Object> m = new HashMap<>();
    JSONObject() {}
    public JSONObject(String s) { JSONObject o = (JSONObject) new Parser(s).value(); m.putAll(o.m); }
    public Object opt(String k) { return m.get(k); }
    public String optString(String k, String d) { Object o = m.get(k); return o instanceof String ? (String) o : d; }
    public int optInt(String k, int d) { Object o = m.get(k); return o instanceof Number ? ((Number) o).intValue() : d; }
    public double optDouble(String k, double d) { Object o = m.get(k); return o instanceof Number ? ((Number) o).doubleValue() : d; }
    public JSONArray optJSONArray(String k) { Object o = m.get(k); return o instanceof JSONArray ? (JSONArray) o : null; }
    public JSONObject optJSONObject(String k) { Object o = m.get(k); return o instanceof JSONObject ? (JSONObject) o : null; }

    static final class Parser {
        final String s; int i;
        Parser(String s) { this.s = s; }
        void ws() { while (i < s.length() && s.charAt(i) <= ' ') i++; }
        Object value() {
            ws();
            char c = s.charAt(i);
            if (c == '{') {
                JSONObject o = new JSONObject(); i++; ws();
                if (s.charAt(i) == '}') { i++; return o; }
                while (true) { ws(); String k = (String) value(); ws(); i++; o.m.put(k, value()); ws(); if (s.charAt(i++) == '}') return o; }
            }
            if (c == '[') {
                JSONArray a = new JSONArray(); i++; ws();
                if (s.charAt(i) == ']') { i++; return a; }
                while (true) { a.l.add(value()); ws(); if (s.charAt(i++) == ']') return a; }
            }
            if (c == '"') {
                StringBuilder b = new StringBuilder(); i++;
                while (s.charAt(i) != '"') { char d = s.charAt(i++); if (d == '\\') { d = s.charAt(i++); if (d == 'n') d = '\n'; } b.append(d); }
                i++; return b.toString();
            }
            int j = i;
            while (i < s.length() && "+-.eE0123456789".indexOf(s.charAt(i)) >= 0) i++;
            if (j == i) { i += 4; return null; }
            String n = s.substring(j, i);
            if (n.contains(".") || n.contains("e") || n.contains("E")) return Double.valueOf(n);
            return Integer.valueOf(n);
        }
    }
}
