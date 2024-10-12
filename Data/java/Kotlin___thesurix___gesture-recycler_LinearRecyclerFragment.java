package com.thesurix.example.gesturerecycler.fragment;

import com.google.android.material.snackbar.Snackbar;

import com.thesurix.example.gesturerecycler.R;
import com.thesurix.example.gesturerecycler.adapter.MonthsAdapter;
import com.thesurix.example.gesturerecycler.model.MonthItem;
import com.thesurix.gesturerecycler.DefaultItemClickListener;
import com.thesurix.gesturerecycler.GestureAdapter;
import com.thesurix.gesturerecycler.GestureManager;
import com.thesurix.gesturerecycler.RecyclerItemTouchListener;

import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.LinearLayoutManager;

public class LinearRecyclerFragment extends BaseFragment {

    @Override
    public void onViewCreated(final View view, @Nullable final Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        final LinearLayoutManager manager = new LinearLayoutManager(getContext());
        mRecyclerView.setHasFixedSize(true);
        mRecyclerView.setLayoutManager(manager);

        final MonthsAdapter adapter = new MonthsAdapter(getContext(), R.layout.linear_item);
        adapter.setData(getMonths());

        mRecyclerView.setAdapter(adapter);
        mRecyclerView.addOnItemTouchListener(new RecyclerItemTouchListener<>(new DefaultItemClickListener<MonthItem>() {

            @Override
            public boolean onItemClick(final MonthItem item, final int position) {
                Snackbar.make(view, "Click event on the " + position + " position", Snackbar.LENGTH_SHORT).show();
                return true;
            }

            @Override
            public void onItemLongPress(final MonthItem item, final int position) {
                Snackbar.make(view, "Long press event on the " + position + " position", Snackbar.LENGTH_SHORT).show();
            }

            @Override
            public boolean onDoubleTap(final MonthItem item, final int position) {
                Snackbar.make(view, "Double tap event on the " + position + " position", Snackbar.LENGTH_SHORT).show();
                return true;
            }
        }));

        mGestureManager = new GestureManager.Builder(mRecyclerView)
                .setSwipeEnabled(true)
                .setLongPressDragEnabled(true)
                .setSwipeFlags(ItemTouchHelper.LEFT | ItemTouchHelper.RIGHT)
                .setDragFlags(ItemTouchHelper.UP | ItemTouchHelper.DOWN)
                .build();

        adapter.setDataChangeListener(new GestureAdapter.OnDataChangeListener<MonthItem>() {
            @Override
            public void onItemRemoved(final MonthItem item, final int position) {
                Snackbar.make(view, "Month removed from position " + position, Snackbar.LENGTH_SHORT).show();
            }

            @Override
            public void onItemReorder(final MonthItem item, final int fromPos, final int toPos) {
                Snackbar.make(view, "Month moved from position " + fromPos + " to " + toPos, Snackbar.LENGTH_SHORT).show();
            }
        });
    }

    @Override
    public boolean onOptionsItemSelected(final MenuItem item) {
        if (item.getItemId() == R.id.recycler_drag_menu) {
            mGestureManager.setManualDragEnabled(!mGestureManager.isManualDragEnabled());
        }
        return super.onOptionsItemSelected(item);
    }
}
