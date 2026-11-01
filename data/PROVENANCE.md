# Data Provenance

## sample.stl
- **Format**: ASCII STL
- **Content**: Unit cube (1m × 1m × 1m) with 12 triangular facets
- **Origin**: Synthetically generated for testing purposes
- **License**: Public domain / CC0
- **Note**: This is a self-created test geometry. No proprietary CAD software was used.

## sample_temperatures.csv
- **Format**: ANSYS-style nodal temperature export (synthetic)
- **Columns**: node_id, x_m, y_m, z_m, temperature_C
- **Content**: 30 synthetic nodes representing a 1D rod temperature profile
- **Temperature range**: 20°C (cold end) to 100°C (hot end)
- **Origin**: Synthetically generated to match the 1D thermal FEA problem (L=1m, T0=100°C, TL=20°C)
- **License**: Synthetic data, no license required
- **Note**: Not an actual ANSYS export; column format matches required schema for testing

## Schema Notes
- node_id: integer, 1-indexed
- x_m, y_m, z_m: position in metres (float)
- temperature_C: temperature in degrees Celsius (float)
- No NaN or infinite values; file passes validation checks
